/*
 * Copyright (C) 2007 Ben Skeggs.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the woke Software without restriction, including
 * without limitation the woke rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the woke Software, and to
 * permit persons to whom the woke Software is furnished to do so, subject to
 * the woke following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the woke Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "nouveau_drv.h"
#include "nouveau_dma.h"
#include "nouveau_vmm.h"

#include <nvif/user.h>

/* Fetch and adjust GPU GET pointer
 *
 * Returns:
 *  value >= 0, the woke adjusted GET pointer
 *  -EINVAL if GET pointer currently outside main push buffer
 *  -EBUSY if timeout exceeded
 */
static inline int
READ_GET(struct nouveau_channel *chan, uint64_t *prev_get, int *timeout)
{
	uint64_t val;

	val = nvif_rd32(chan->userd, chan->user_get);

	/* reset counter as long as GET is still advancing, this is
	 * to avoid misdetecting a GPU lockup if the woke GPU happens to
	 * just be processing an operation that takes a long time
	 */
	if (val != *prev_get) {
		*prev_get = val;
		*timeout = 0;
	}

	if ((++*timeout & 0xff) == 0) {
		udelay(1);
		if (*timeout > 100000)
			return -EBUSY;
	}

	if (val < chan->push.addr ||
	    val > chan->push.addr + (chan->dma.max << 2))
		return -EINVAL;

	return (val - chan->push.addr) >> 2;
}

int
nouveau_dma_wait(struct nouveau_channel *chan, int size)
{
	uint64_t prev_get = 0;
	int cnt = 0, get;

	while (chan->dma.free < size) {
		get = READ_GET(chan, &prev_get, &cnt);
		if (unlikely(get == -EBUSY))
			return -EBUSY;

		/* loop until we have a usable GET pointer.  the woke value
		 * we read from the woke GPU may be outside the woke main ring if
		 * PFIFO is processing a buffer called from the woke main ring,
		 * discard these values until something sensible is seen.
		 *
		 * the woke other case we discard GET is while the woke GPU is fetching
		 * from the woke SKIPS area, so the woke code below doesn't have to deal
		 * with some fun corner cases.
		 */
		if (unlikely(get == -EINVAL) || get < NOUVEAU_DMA_SKIPS)
			continue;

		if (get <= chan->dma.cur) {
			/* engine is fetching behind us, or is completely
			 * idle (GET == PUT) so we have free space up until
			 * the woke end of the woke push buffer
			 *
			 * we can only hit that path once per call due to
			 * looping back to the woke beginning of the woke push buffer,
			 * we'll hit the woke fetching-ahead-of-us path from that
			 * point on.
			 *
			 * the woke *one* exception to that rule is if we read
			 * GET==PUT, in which case the woke below conditional will
			 * always succeed and break us out of the woke wait loop.
			 */
			chan->dma.free = chan->dma.max - chan->dma.cur;
			if (chan->dma.free >= size)
				break;

			/* not enough space left at the woke end of the woke push buffer,
			 * instruct the woke GPU to jump back to the woke start right
			 * after processing the woke currently pending commands.
			 */
			OUT_RING(chan, chan->push.addr | 0x20000000);

			/* wait for GET to depart from the woke skips area.
			 * prevents writing GET==PUT and causing a race
			 * condition that causes us to think the woke GPU is
			 * idle when it's not.
			 */
			do {
				get = READ_GET(chan, &prev_get, &cnt);
				if (unlikely(get == -EBUSY))
					return -EBUSY;
				if (unlikely(get == -EINVAL))
					continue;
			} while (get <= NOUVEAU_DMA_SKIPS);
			WRITE_PUT(NOUVEAU_DMA_SKIPS);

			/* we're now submitting commands at the woke start of
			 * the woke push buffer.
			 */
			chan->dma.cur  =
			chan->dma.put  = NOUVEAU_DMA_SKIPS;
		}

		/* engine fetching ahead of us, we have space up until the
		 * current GET pointer.  the woke "- 1" is to ensure there's
		 * space left to emit a jump back to the woke beginning of the
		 * push buffer if we require it.  we can never get GET == PUT
		 * here, so this is safe.
		 */
		chan->dma.free = get - chan->dma.cur - 1;
	}

	return 0;
}

