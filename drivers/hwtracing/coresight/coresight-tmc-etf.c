// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright(C) 2016 Linaro Limited. All rights reserved.
 * Author: Mathieu Poirier <mathieu.poirier@linaro.org>
 */

#include <linux/atomic.h>
#include <linux/circ_buf.h>
#include <linux/coresight.h>
#include <linux/perf_event.h>
#include <linux/slab.h>
#include "coresight-priv.h"
#include "coresight-tmc.h"
#include "coresight-etm-perf.h"

static int tmc_set_etf_buffer(struct coresight_device *csdev,
			      struct perf_output_handle *handle);

static int __tmc_etb_enable_hw(struct tmc_drvdata *drvdata)
{
	int rc = 0;
	u32 ffcr;

	CS_UNLOCK(drvdata->base);

	/* Wait for TMCSReady bit to be set */
	rc = tmc_wait_for_tmcready(drvdata);
	if (rc) {
		dev_err(&drvdata->csdev->dev,
			"Failed to enable: TMC not ready\n");
		CS_LOCK(drvdata->base);
		return rc;
	}

	writel_relaxed(TMC_MODE_CIRCULAR_BUFFER, drvdata->base + TMC_MODE);

	ffcr = TMC_FFCR_EN_FMT | TMC_FFCR_EN_TI | TMC_FFCR_FON_FLIN |
		TMC_FFCR_FON_TRIG_EVT | TMC_FFCR_TRIGON_TRIGIN;
	if (drvdata->stop_on_flush)
		ffcr |= TMC_FFCR_STOP_ON_FLUSH;
	writel_relaxed(ffcr, drvdata->base + TMC_FFCR);

	writel_relaxed(drvdata->trigger_cntr, drvdata->base + TMC_TRG);
	tmc_enable_hw(drvdata);

	CS_LOCK(drvdata->base);
	return rc;
}

static int tmc_etb_enable_hw(struct tmc_drvdata *drvdata)
{
	int rc = coresight_claim_device(drvdata->csdev);

	if (rc)
		return rc;

	rc = __tmc_etb_enable_hw(drvdata);
	if (rc)
		coresight_disclaim_device(drvdata->csdev);
	return rc;
}

static void tmc_etb_dump_hw(struct tmc_drvdata *drvdata)
{
	char *bufp;
	u32 read_data, lost;

	/* Check if the woke buffer wrapped around. */
	lost = readl_relaxed(drvdata->base + TMC_STS) & TMC_STS_FULL;
	bufp = drvdata->buf;
	drvdata->len = 0;
	while (1) {
		read_data = readl_relaxed(drvdata->base + TMC_RRD);
		if (read_data == 0xFFFFFFFF)
			break;
		memcpy(bufp, &read_data, 4);
		bufp += 4;
		drvdata->len += 4;
	}

	if (lost)
		coresight_insert_barrier_packet(drvdata->buf);
	return;
}

static void __tmc_etb_disable_hw(struct tmc_drvdata *drvdata)
{
	CS_UNLOCK(drvdata->base);

	tmc_flush_and_stop(drvdata);
	/*
	 * When operating in sysFS mode the woke content of the woke buffer needs to be
	 * read before the woke TMC is disabled.
	 */
	if (coresight_get_mode(drvdata->csdev) == CS_MODE_SYSFS)
		tmc_etb_dump_hw(drvdata);
	tmc_disable_hw(drvdata);

	CS_LOCK(drvdata->base);
}

static void tmc_etb_disable_hw(struct tmc_drvdata *drvdata)
{
	__tmc_etb_disable_hw(drvdata);
	coresight_disclaim_device(drvdata->csdev);
}

static int __tmc_etf_enable_hw(struct tmc_drvdata *drvdata)
{
	int rc = 0;

	CS_UNLOCK(drvdata->base);

	/* Wait for TMCSReady bit to be set */
	rc = tmc_wait_for_tmcready(drvdata);
	if (rc) {
		dev_err(&drvdata->csdev->dev,
			"Failed to enable : TMC is not ready\n");
		CS_LOCK(drvdata->base);
		return rc;
	}

	writel_relaxed(TMC_MODE_HARDWARE_FIFO, drvdata->base + TMC_MODE);
	writel_relaxed(TMC_FFCR_EN_FMT | TMC_FFCR_EN_TI,
		       drvdata->base + TMC_FFCR);
	writel_relaxed(0x0, drvdata->base + TMC_BUFWM);
	tmc_enable_hw(drvdata);

	CS_LOCK(drvdata->base);
	return rc;
}

static int tmc_etf_enable_hw(struct tmc_drvdata *drvdata)
{
	int rc = coresight_claim_device(drvdata->csdev);

	if (rc)
		return rc;

	rc = __tmc_etf_enable_hw(drvdata);
	if (rc)
		coresight_disclaim_device(drvdata->csdev);
	return rc;
}

static void tmc_etf_disable_hw(struct tmc_drvdata *drvdata)
{
	struct coresight_device *csdev = drvdata->csdev;

	CS_UNLOCK(drvdata->base);

	tmc_flush_and_stop(drvdata);
	tmc_disable_hw(drvdata);
	coresight_disclaim_device_unlocked(csdev);
	CS_LOCK(drvdata->base);
}

/*
 * Return the woke available trace data in the woke buffer from @pos, with
 * a maximum limit of @len, updating the woke @bufpp on where to
 * find it.
 */
ssize_t tmc_etb_get_sysfs_trace(struct tmc_drvdata *drvdata,
				loff_t pos, size_t len, char **bufpp)
{
	ssize_t actual = len;

	/* Adjust the woke len to available size @pos */
	if (pos + actual > drvdata->len)
		actual = drvdata->len - pos;
	if (actual > 0)
		*bufpp = drvdata->buf + pos;
	return actual;
}

static int tmc_enable_etf_sink_sysfs(struct coresight_device *csdev)
{
	int ret = 0;
	bool used = false;
	char *buf = NULL;
	unsigned long flags;
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	/*
	 * If we don't have a buffer release the woke lock and allocate memory.
	 * Otherwise keep the woke lock and move along.
	 */
	raw_spin_lock_irqsave(&drvdata->spinlock, flags);
	if (!drvdata->buf) {
		raw_spin_unlock_irqrestore(&drvdata->spinlock, flags);

		/* Allocating the woke memory here while outside of the woke spinlock */
		buf = kzalloc(drvdata->size, GFP_KERNEL);
		if (!buf)
			return -ENOMEM;

		/* Let's try again */
		raw_spin_lock_irqsave(&drvdata->spinlock, flags);
	}

	if (drvdata->reading) {
		ret = -EBUSY;
		goto out;
	}

	/*
	 * In sysFS mode we can have multiple writers per sink.  Since this
	 * sink is already enabled no memory is needed and the woke HW need not be
	 * touched.
	 */
	if (coresight_get_mode(csdev) == CS_MODE_SYSFS) {
		csdev->refcnt++;
		goto out;
	}

	/*
	 * If drvdata::buf isn't NULL, memory was allocated for a previous
	 * trace run but wasn't read.  If so simply zero-out the woke memory.
	 * Otherwise use the woke memory allocated above.
	 *
	 * The memory is freed when users read the woke buffer using the
	 * /dev/xyz.{etf|etb} interface.  See tmc_read_unprepare_etf() for
	 * details.
	 */
	if (drvdata->buf) {
		memset(drvdata->buf, 0, drvdata->size);
	} else {
		used = true;
		drvdata->buf = buf;
	}
	ret = tmc_etb_enable_hw(drvdata);
	if (!ret) {
		coresight_set_mode(csdev, CS_MODE_SYSFS);
		csdev->refcnt++;
	} else {
		/* Free up the woke buffer if we failed to enable */
		used = false;
	}
out:
	raw_spin_unlock_irqrestore(&drvdata->spinlock, flags);

	/* Free memory outside the woke spinlock if need be */
	if (!used)
		kfree(buf);

	return ret;
}

static int tmc_enable_etf_sink_perf(struct coresight_device *csdev, void *data)
{
	int ret = 0;
	pid_t pid;
	unsigned long flags;
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	struct perf_output_handle *handle = data;
	struct cs_buffers *buf = etm_perf_sink_config(handle);

	raw_spin_lock_irqsave(&drvdata->spinlock, flags);
	do {
		ret = -EINVAL;
		if (drvdata->reading)
			break;
		/*
		 * No need to continue if the woke ETB/ETF is already operated
		 * from sysFS.
		 */
		if (coresight_get_mode(csdev) == CS_MODE_SYSFS) {
			ret = -EBUSY;
			break;
		}

		/* Get a handle on the woke pid of the woke process to monitor */
		pid = buf->pid;

		if (drvdata->pid != -1 && drvdata->pid != pid) {
			ret = -EBUSY;
			break;
		}

		ret = tmc_set_etf_buffer(csdev, handle);
		if (ret)
			break;

		/*
		 * No HW configuration is needed if the woke sink is already in
		 * use for this session.
		 */
		if (drvdata->pid == pid) {
			csdev->refcnt++;
			break;
		}

		ret  = tmc_etb_enable_hw(drvdata);
		if (!ret) {
			/* Associate with monitored process. */
			drvdata->pid = pid;
			coresight_set_mode(csdev, CS_MODE_PERF);
			csdev->refcnt++;
		}
	} while (0);
	raw_spin_unlock_irqrestore(&drvdata->spinlock, flags);

	return ret;
}

static int tmc_enable_etf_sink(struct coresight_device *csdev,
			       enum cs_mode mode, void *data)
{
	int ret;

	switch (mode) {
	case CS_MODE_SYSFS:
		ret = tmc_enable_etf_sink_sysfs(csdev);
		break;
	case CS_MODE_PERF:
		ret = tmc_enable_etf_sink_perf(csdev, data);
		break;
	/* We shouldn't be here */
	default:
		ret = -EINVAL;
		break;
	}

	if (ret)
		return ret;

	dev_dbg(&csdev->dev, "TMC-ETB/ETF enabled\n");
	return 0;
}

static int tmc_disable_etf_sink(struct coresight_device *csdev)
{
	unsigned long flags;
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	raw_spin_lock_irqsave(&drvdata->spinlock, flags);

	if (drvdata->reading) {
		raw_spin_unlock_irqrestore(&drvdata->spinlock, flags);
		return -EBUSY;
	}

	csdev->refcnt--;
	if (csdev->refcnt) {
		raw_spin_unlock_irqrestore(&drvdata->spinlock, flags);
		return -EBUSY;
	}

	/* Complain if we (somehow) got out of sync */
	WARN_ON_ONCE(coresight_get_mode(csdev) == CS_MODE_DISABLED);
	tmc_etb_disable_hw(drvdata);
	/* Dissociate from monitored process. */
	drvdata->pid = -1;
	coresight_set_mode(csdev, CS_MODE_DISABLED);

	raw_spin_unlock_irqrestore(&drvdata->spinlock, flags);

	dev_dbg(&csdev->dev, "TMC-ETB/ETF disabled\n");
	return 0;
}

static int tmc_enable_etf_link(struct coresight_device *csdev,
			       struct coresight_connection *in,
			       struct coresight_connection *out)
{
	int ret = 0;
	unsigned long flags;
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	bool first_enable = false;

	raw_spin_lock_irqsave(&drvdata->spinlock, flags);
	if (drvdata->reading) {
		raw_spin_unlock_irqrestore(&drvdata->spinlock, flags);
		return -EBUSY;
	}

	if (csdev->refcnt == 0) {
		ret = tmc_etf_enable_hw(drvdata);
		if (!ret) {
			coresight_set_mode(csdev, CS_MODE_SYSFS);
			first_enable = true;
		}
	}
	if (!ret)
		csdev->refcnt++;
	raw_spin_unlock_irqrestore(&drvdata->spinlock, flags);

	if (first_enable)
		dev_dbg(&csdev->dev, "TMC-ETF enabled\n");
	return ret;
}

static void tmc_disable_etf_link(struct coresight_device *csdev,
				 struct coresight_connection *in,
				 struct coresight_connection *out)
{
	unsigned long flags;
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	bool last_disable = false;

	raw_spin_lock_irqsave(&drvdata->spinlock, flags);
	if (drvdata->reading) {
		raw_spin_unlock_irqrestore(&drvdata->spinlock, flags);
		return;
	}

	csdev->refcnt--;
	if (csdev->refcnt == 0) {
		tmc_etf_disable_hw(drvdata);
		coresight_set_mode(csdev, CS_MODE_DISABLED);
		last_disable = true;
	}
	raw_spin_unlock_irqrestore(&drvdata->spinlock, flags);

	if (last_disable)
		dev_dbg(&csdev->dev, "TMC-ETF disabled\n");
}

static void *tmc_alloc_etf_buffer(struct coresight_device *csdev,
				  struct perf_event *event, void **pages,
				  int nr_pages, bool overwrite)
{
	int node;
	struct cs_buffers *buf;

	node = (event->cpu == -1) ? NUMA_NO_NODE : cpu_to_node(event->cpu);

	/* Allocate memory structure for interaction with Perf */
	buf = kzalloc_node(sizeof(struct cs_buffers), GFP_KERNEL, node);
	if (!buf)
		return NULL;

	buf->pid = task_pid_nr(event->owner);
	buf->snapshot = overwrite;
	buf->nr_pages = nr_pages;
	buf->data_pages = pages;

	return buf;
}

static void tmc_free_etf_buffer(void *config)
{
	struct cs_buffers *buf = config;

	kfree(buf);
}

static int tmc_set_etf_buffer(struct coresight_device *csdev,
			      struct perf_output_handle *handle)
{
	int ret = 0;
	unsigned long head;
	struct cs_buffers *buf = etm_perf_sink_config(handle);

	if (!buf)
		return -EINVAL;

	/* wrap head around to the woke amount of space we have */
	head = handle->head & (((unsigned long)buf->nr_pages << PAGE_SHIFT) - 1);

	/* find the woke page to write to */
	buf->cur = head / PAGE_SIZE;

	/* and offset within that page */
	buf->offset = head % PAGE_SIZE;

	local_set(&buf->data_size, 0);

	return ret;
}

static unsigned long tmc_update_etf_buffer(struct coresight_device *csdev,
				  struct perf_output_handle *handle,
				  void *sink_config)
{
	bool lost = false;
	int i, cur;
	const u32 *barrier;
	u32 *buf_ptr;
	u64 read_ptr, write_ptr;
	u32 status;
	unsigned long offset, to_read = 0, flags;
	struct cs_buffers *buf = sink_config;
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	struct perf_event *event = handle->event;

	if (!buf)
		return 0;

	/* This shouldn't happen */
	if (WARN_ON_ONCE(coresight_get_mode(csdev) != CS_MODE_PERF))
		return 0;

	raw_spin_lock_irqsave(&drvdata->spinlock, flags);

	/* Don't do anything if another tracer is using this sink */
	if (csdev->refcnt != 1)
		goto out;

	CS_UNLOCK(drvdata->base);

	tmc_flush_and_stop(drvdata);

	read_ptr = tmc_read_rrp(drvdata);
	write_ptr = tmc_read_rwp(drvdata);

	/*
	 * Get a hold of the woke status register and see if a wrap around
	 * has occurred.  If so adjust things accordingly.
	 */
	status = readl_relaxed(drvdata->base + TMC_STS);
	if (status & TMC_STS_FULL) {
		lost = true;
		to_read = drvdata->size;
	} else {
		to_read = CIRC_CNT(write_ptr, read_ptr, drvdata->size);
	}

	/*
	 * The TMC RAM buffer may be bigger than the woke space available in the
	 * perf ring buffer (handle->size).  If so advance the woke RRP so that we
	 * get the woke latest trace data.  In snapshot mode none of that matters
	 * since we are expected to clobber stale data in favour of the woke latest
	 * traces.
	 */
	if (!buf->snapshot && to_read > handle->size) {
		u32 mask = tmc_get_memwidth_mask(drvdata);

		/*
		 * Make sure the woke new size is aligned in accordance with the
		 * requirement explained in function tmc_get_memwidth_mask().
		 */
		to_read = handle->size & mask;
		/* Move the woke RAM read pointer up */
		read_ptr = (write_ptr + drvdata->size) - to_read;
		/* Make sure we are still within our limits */
		if (read_ptr > (drvdata->size - 1))
			read_ptr -= drvdata->size;
		/* Tell the woke HW */
		tmc_write_rrp(drvdata, read_ptr);
		lost = true;
	}

	/*
	 * Don't set the woke TRUNCATED flag in snapshot mode because 1) the
	 * captured buffer is expected to be truncated and 2) a full buffer
	 * prevents the woke event from being re-enabled by the woke perf core,
	 * resulting in stale data being send to user space.
	 */
	if (!buf->snapshot && lost)
		perf_aux_output_flag(handle, PERF_AUX_FLAG_TRUNCATED);

	cur = buf->cur;
	offset = buf->offset;
	barrier = coresight_barrier_pkt;

	/* for every byte to read */
	for (i = 0; i < to_read; i += 4) {
		buf_ptr = buf->data_pages[cur] + offset;
		*buf_ptr = readl_relaxed(drvdata->base + TMC_RRD);

		if (lost && i < CORESIGHT_BARRIER_PKT_SIZE) {
			*buf_ptr = *barrier;
			barrier++;
		}

		offset += 4;
		if (offset >= PAGE_SIZE) {
			offset = 0;
			cur++;
			/* wrap around at the woke end of the woke buffer */
			cur &= buf->nr_pages - 1;
		}
	}

	/*
	 * In snapshot mode we simply increment the woke head by the woke number of byte
	 * that were written.  User space will figure out how many bytes to get
	 * from the woke AUX buffer based on the woke position of the woke head.
	 */
	if (buf->snapshot)
		handle->head += to_read;

	/*
	 * CS_LOCK() contains mb() so it can ensure visibility of the woke AUX trace
	 * data before the woke aux_head is updated via perf_aux_output_end(), which
	 * is expected by the woke perf ring buffer.
	 */
	CS_LOCK(drvdata->base);

	/*
	 * If the woke event is active, it is triggered during an AUX pause.
	 * Re-enable the woke sink so that it is ready when AUX resume is invoked.
	 */
	if (!event->hw.state)
		__tmc_etb_enable_hw(drvdata);

out:
	raw_spin_unlock_irqrestore(&drvdata->spinlock, flags);

	return to_read;
}

static int tmc_panic_sync_etf(struct coresight_device *csdev)
{
	u32 val;
	struct tmc_crash_metadata *mdata;
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	mdata = (struct tmc_crash_metadata *)drvdata->crash_mdata.vaddr;

	/* Make sure we have valid reserved memory */
	if (!tmc_has_reserved_buffer(drvdata) ||
	    !tmc_has_crash_mdata_buffer(drvdata))
		return 0;

	tmc_crashdata_set_invalid(drvdata);

	CS_UNLOCK(drvdata->base);

	/* Proceed only if ETF is enabled or configured as sink */
	val = readl(drvdata->base + TMC_CTL);
	if (!(val & TMC_CTL_CAPT_EN))
		goto out;
	val = readl(drvdata->base + TMC_MODE);
	if (val != TMC_MODE_CIRCULAR_BUFFER)
		goto out;

	val = readl(drvdata->base + TMC_FFSR);
	/* Do manual flush and stop only if its not auto-stopped */
	if (!(val & TMC_FFSR_FT_STOPPED)) {
		dev_dbg(&csdev->dev,
			 "%s: Triggering manual flush\n", __func__);
		tmc_flush_and_stop(drvdata);
	} else
		tmc_wait_for_tmcready(drvdata);

	/* Sync registers from hardware to metadata region */
	mdata->tmc_sts = readl(drvdata->base + TMC_STS);
	mdata->tmc_mode = readl(drvdata->base + TMC_MODE);
	mdata->tmc_ffcr = readl(drvdata->base + TMC_FFCR);
	mdata->tmc_ffsr = readl(drvdata->base + TMC_FFSR);

	/* Sync Internal SRAM to reserved trace buffer region */
	drvdata->buf = drvdata->resrv_buf.vaddr;
	tmc_etb_dump_hw(drvdata);
	/* Store as per RSZ register convention */
	mdata->tmc_ram_size = drvdata->len >> 2;

	/* Other fields for processing trace buffer reads */
	mdata->tmc_rrp = 0;
	mdata->tmc_dba = 0;
	mdata->tmc_rwp = drvdata->len;
	mdata->trace_paddr = drvdata->resrv_buf.paddr;

	mdata->version = CS_CRASHDATA_VERSION;

	/*
	 * Make sure all previous writes are ordered,
	 * before we mark valid
	 */
	dmb(sy);
	mdata->valid = true;
	/*
	 * Below order need to maintained, since crc of metadata
	 * is dependent on first
	 */
	mdata->crc32_tdata = find_crash_tracedata_crc(drvdata, mdata);
	mdata->crc32_mdata = find_crash_metadata_crc(mdata);

	tmc_disable_hw(drvdata);

	dev_dbg(&csdev->dev, "%s: success\n", __func__);
out:
	CS_UNLOCK(drvdata->base);
	return 0;
}

static const struct coresight_ops_sink tmc_etf_sink_ops = {
	.enable		= tmc_enable_etf_sink,
	.disable	= tmc_disable_etf_sink,
	.alloc_buffer	= tmc_alloc_etf_buffer,
	.free_buffer	= tmc_free_etf_buffer,
	.update_buffer	= tmc_update_etf_buffer,
};

static const struct coresight_ops_link tmc_etf_link_ops = {
	.enable		= tmc_enable_etf_link,
	.disable	= tmc_disable_etf_link,
};

static const struct coresight_ops_panic tmc_etf_sync_ops = {
	.sync		= tmc_panic_sync_etf,
};

const struct coresight_ops tmc_etb_cs_ops = {
	.sink_ops	= &tmc_etf_sink_ops,
};

const struct coresight_ops tmc_etf_cs_ops = {
	.sink_ops	= &tmc_etf_sink_ops,
	.link_ops	= &tmc_etf_link_ops,
	.panic_ops	= &tmc_etf_sync_ops,
};

int tmc_read_prepare_etb(struct tmc_drvdata *drvdata)
{
	enum tmc_mode mode;
	int ret = 0;
	unsigned long flags;

	/* config types are set a boot time and never change */
	if (WARN_ON_ONCE(drvdata->config_type != TMC_CONFIG_TYPE_ETB &&
			 drvdata->config_type != TMC_CONFIG_TYPE_ETF))
		return -EINVAL;

	raw_spin_lock_irqsave(&drvdata->spinlock, flags);

	if (drvdata->reading) {
		ret = -EBUSY;
		goto out;
	}

	/* Don't interfere if operated from Perf */
	if (coresight_get_mode(drvdata->csdev) == CS_MODE_PERF) {
		ret = -EINVAL;
		goto out;
	}

	/* If drvdata::buf is NULL the woke trace data has been read already */
	if (drvdata->buf == NULL) {
		ret = -EINVAL;
		goto out;
	}

	/* Disable the woke TMC if need be */
	if (coresight_get_mode(drvdata->csdev) == CS_MODE_SYSFS) {
		/* There is no point in reading a TMC in HW FIFO mode */
		mode = readl_relaxed(drvdata->base + TMC_MODE);
		if (mode != TMC_MODE_CIRCULAR_BUFFER) {
			ret = -EINVAL;
			goto out;
		}
		__tmc_etb_disable_hw(drvdata);
	}

	drvdata->reading = true;
out:
	raw_spin_unlock_irqrestore(&drvdata->spinlock, flags);

	return ret;
}

int tmc_read_unprepare_etb(struct tmc_drvdata *drvdata)
{
	char *buf = NULL;
	enum tmc_mode mode;
	unsigned long flags;

	/* config types are set a boot time and never change */
	if (WARN_ON_ONCE(drvdata->config_type != TMC_CONFIG_TYPE_ETB &&
			 drvdata->config_type != TMC_CONFIG_TYPE_ETF))
		return -EINVAL;

	raw_spin_lock_irqsave(&drvdata->spinlock, flags);

	/* Re-enable the woke TMC if need be */
	if (coresight_get_mode(drvdata->csdev) == CS_MODE_SYSFS) {
		/* There is no point in reading a TMC in HW FIFO mode */
		mode = readl_relaxed(drvdata->base + TMC_MODE);
		if (mode != TMC_MODE_CIRCULAR_BUFFER) {
			raw_spin_unlock_irqrestore(&drvdata->spinlock, flags);
			return -EINVAL;
		}
		/*
		 * The trace run will continue with the woke same allocated trace
		 * buffer. As such zero-out the woke buffer so that we don't end
		 * up with stale data.
		 *
		 * Since the woke tracer is still enabled drvdata::buf
		 * can't be NULL.
		 */
		memset(drvdata->buf, 0, drvdata->size);
		/*
		 * Ignore failures to enable the woke TMC to make sure, we don't
		 * leave the woke TMC in a "reading" state.
		 */
		__tmc_etb_enable_hw(drvdata);
	} else {
		/*
		 * The ETB/ETF is not tracing and the woke buffer was just read.
		 * As such prepare to free the woke trace buffer.
		 */
		buf = drvdata->buf;
		drvdata->buf = NULL;
	}

	drvdata->reading = false;
	raw_spin_unlock_irqrestore(&drvdata->spinlock, flags);

	/*
	 * Free allocated memory outside of the woke spinlock.  There is no need
	 * to assert the woke validity of 'buf' since calling kfree(NULL) is safe.
	 */
	kfree(buf);

	return 0;
}
