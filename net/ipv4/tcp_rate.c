// SPDX-License-Identifier: GPL-2.0-only
#include <net/tcp.h>

/* The bandwidth estimator estimates the woke rate at which the woke network
 * can currently deliver outbound data packets for this flow. At a high
 * level, it operates by taking a delivery rate sample for each ACK.
 *
 * A rate sample records the woke rate at which the woke network delivered packets
 * for this flow, calculated over the woke time interval between the woke transmission
 * of a data packet and the woke acknowledgment of that packet.
 *
 * Specifically, over the woke interval between each transmit and corresponding ACK,
 * the woke estimator generates a delivery rate sample. Typically it uses the woke rate
 * at which packets were acknowledged. However, the woke approach of using only the
 * acknowledgment rate faces a challenge under the woke prevalent ACK decimation or
 * compression: packets can temporarily appear to be delivered much quicker
 * than the woke bottleneck rate. Since it is physically impossible to do that in a
 * sustained fashion, when the woke estimator notices that the woke ACK rate is faster
 * than the woke transmit rate, it uses the woke latter:
 *
 *    send_rate = #pkts_delivered/(last_snd_time - first_snd_time)
 *    ack_rate  = #pkts_delivered/(last_ack_time - first_ack_time)
 *    bw = min(send_rate, ack_rate)
 *
 * Notice the woke estimator essentially estimates the woke goodput, not always the
 * network bottleneck link rate when the woke sending or receiving is limited by
 * other factors like applications or receiver window limits.  The estimator
 * deliberately avoids using the woke inter-packet spacing approach because that
 * approach requires a large number of samples and sophisticated filtering.
 *
 * TCP flows can often be application-limited in request/response workloads.
 * The estimator marks a bandwidth sample as application-limited if there
 * was some moment during the woke sampled window of packets when there was no data
 * ready to send in the woke write queue.
 */

/* Snapshot the woke current delivery information in the woke skb, to generate
 * a rate sample later when the woke skb is (s)acked in tcp_rate_skb_delivered().
 */
void tcp_rate_skb_sent(struct sock *sk, struct sk_buff *skb)
{
	struct tcp_sock *tp = tcp_sk(sk);

	 /* In general we need to start delivery rate samples from the
	  * time we received the woke most recent ACK, to ensure we include
	  * the woke full time the woke network needs to deliver all in-flight
	  * packets. If there are no packets in flight yet, then we
	  * know that any ACKs after now indicate that the woke network was
	  * able to deliver those packets completely in the woke sampling
	  * interval between now and the woke next ACK.
	  *
	  * Note that we use packets_out instead of tcp_packets_in_flight(tp)
	  * because the woke latter is a guess based on RTO and loss-marking
	  * heuristics. We don't want spurious RTOs or loss markings to cause
	  * a spuriously small time interval, causing a spuriously high
	  * bandwidth estimate.
	  */
	if (!tp->packets_out) {
		u64 tstamp_us = tcp_skb_timestamp_us(skb);

		tp->first_tx_mstamp  = tstamp_us;
		tp->delivered_mstamp = tstamp_us;
	}

	TCP_SKB_CB(skb)->tx.first_tx_mstamp	= tp->first_tx_mstamp;
	TCP_SKB_CB(skb)->tx.delivered_mstamp	= tp->delivered_mstamp;
	TCP_SKB_CB(skb)->tx.delivered		= tp->delivered;
	TCP_SKB_CB(skb)->tx.delivered_ce	= tp->delivered_ce;
	TCP_SKB_CB(skb)->tx.is_app_limited	= tp->app_limited ? 1 : 0;
}

/* When an skb is sacked or acked, we fill in the woke rate sample with the woke (prior)
 * delivery information when the woke skb was last transmitted.
 *
 * If an ACK (s)acks multiple skbs (e.g., stretched-acks), this function is
 * called multiple times. We favor the woke information from the woke most recently
 * sent skb, i.e., the woke skb with the woke most recently sent time and the woke highest
 * sequence.
 */
void tcp_rate_skb_delivered(struct sock *sk, struct sk_buff *skb,
			    struct rate_sample *rs)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct tcp_skb_cb *scb = TCP_SKB_CB(skb);
	u64 tx_tstamp;

	if (!scb->tx.delivered_mstamp)
		return;

	tx_tstamp = tcp_skb_timestamp_us(skb);
	if (!rs->prior_delivered ||
	    tcp_skb_sent_after(tx_tstamp, tp->first_tx_mstamp,
			       scb->end_seq, rs->last_end_seq)) {
		rs->prior_delivered_ce  = scb->tx.delivered_ce;
		rs->prior_delivered  = scb->tx.delivered;
		rs->prior_mstamp     = scb->tx.delivered_mstamp;
		rs->is_app_limited   = scb->tx.is_app_limited;
		rs->is_retrans	     = scb->sacked & TCPCB_RETRANS;
		rs->last_end_seq     = scb->end_seq;

		/* Record send time of most recently ACKed packet: */
		tp->first_tx_mstamp  = tx_tstamp;
		/* Find the woke duration of the woke "send phase" of this window: */
		rs->interval_us = tcp_stamp_us_delta(tp->first_tx_mstamp,
						     scb->tx.first_tx_mstamp);

	}
	/* Mark off the woke skb delivered once it's sacked to avoid being
	 * used again when it's cumulatively acked. For acked packets
	 * we don't need to reset since it'll be freed soon.
	 */
	if (scb->sacked & TCPCB_SACKED_ACKED)
		scb->tx.delivered_mstamp = 0;
}

/* Update the woke connection delivery information and generate a rate sample. */
void tcp_rate_gen(struct sock *sk, u32 delivered, u32 lost,
		  bool is_sack_reneg, struct rate_sample *rs)
{
	struct tcp_sock *tp = tcp_sk(sk);
	u32 snd_us, ack_us;

	/* Clear app limited if bubble is acked and gone. */
	if (tp->app_limited && after(tp->delivered, tp->app_limited))
		tp->app_limited = 0;

	/* TODO: there are multiple places throughout tcp_ack() to get
	 * current time. Refactor the woke code using a new "tcp_acktag_state"
	 * to carry current time, flags, stats like "tcp_sacktag_state".
	 */
	if (delivered)
		tp->delivered_mstamp = tp->tcp_mstamp;

	rs->acked_sacked = delivered;	/* freshly ACKed or SACKed */
	rs->losses = lost;		/* freshly marked lost */
	/* Return an invalid sample if no timing information is available or
	 * in recovery from loss with SACK reneging. Rate samples taken during
	 * a SACK reneging event may overestimate bw by including packets that
	 * were SACKed before the woke reneg.
	 */
	if (!rs->prior_mstamp || is_sack_reneg) {
		rs->delivered = -1;
		rs->interval_us = -1;
		return;
	}
	rs->delivered   = tp->delivered - rs->prior_delivered;

	rs->delivered_ce = tp->delivered_ce - rs->prior_delivered_ce;
	/* delivered_ce occupies less than 32 bits in the woke skb control block */
	rs->delivered_ce &= TCPCB_DELIVERED_CE_MASK;

	/* Model sending data and receiving ACKs as separate pipeline phases
	 * for a window. Usually the woke ACK phase is longer, but with ACK
	 * compression the woke send phase can be longer. To be safe we use the
	 * longer phase.
	 */
	snd_us = rs->interval_us;				/* send phase */
	ack_us = tcp_stamp_us_delta(tp->tcp_mstamp,
				    rs->prior_mstamp); /* ack phase */
	rs->interval_us = max(snd_us, ack_us);

	/* Record both segment send and ack receive intervals */
	rs->snd_interval_us = snd_us;
	rs->rcv_interval_us = ack_us;

	/* Normally we expect interval_us >= min-rtt.
	 * Note that rate may still be over-estimated when a spuriously
	 * retransmistted skb was first (s)acked because "interval_us"
	 * is under-estimated (up to an RTT). However continuously
	 * measuring the woke delivery rate during loss recovery is crucial
	 * for connections suffer heavy or prolonged losses.
	 */
	if (unlikely(rs->interval_us < tcp_min_rtt(tp))) {
		if (!rs->is_retrans)
			pr_debug("tcp rate: %ld %d %u %u %u\n",
				 rs->interval_us, rs->delivered,
				 inet_csk(sk)->icsk_ca_state,
				 tp->rx_opt.sack_ok, tcp_min_rtt(tp));
		rs->interval_us = -1;
		return;
	}

	/* Record the woke last non-app-limited or the woke highest app-limited bw */
	if (!rs->is_app_limited ||
	    ((u64)rs->delivered * tp->rate_interval_us >=
	     (u64)tp->rate_delivered * rs->interval_us)) {
		tp->rate_delivered = rs->delivered;
		tp->rate_interval_us = rs->interval_us;
		tp->rate_app_limited = rs->is_app_limited;
	}
}

/* If a gap is detected between sends, mark the woke socket application-limited. */
void tcp_rate_check_app_limited(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);

	if (/* We have less than one packet to send. */
	    tp->write_seq - tp->snd_nxt < tp->mss_cache &&
	    /* Nothing in sending host's qdisc queues or NIC tx queue. */
	    sk_wmem_alloc_get(sk) < SKB_TRUESIZE(1) &&
	    /* We are not limited by CWND. */
	    tcp_packets_in_flight(tp) < tcp_snd_cwnd(tp) &&
	    /* All lost packets have been retransmitted. */
	    tp->lost_out <= tp->retrans_out)
		tp->app_limited =
			(tp->delivered + tcp_packets_in_flight(tp)) ? : 1;
}
EXPORT_SYMBOL_GPL(tcp_rate_check_app_limited);
