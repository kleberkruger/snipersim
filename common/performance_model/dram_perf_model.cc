#include "simulator.h"
#include "config.h"
#include "dram_perf_model.h"
#include "queue_model_history_list.h"
#include "config.hpp"
#include "stats.h"

#include <iostream>

// Note: Each Dram Controller owns a single DramModel object
// Hence, m_dram_bandwidth is the bandwidth for a single DRAM controller
// Total Bandwidth = m_dram_bandwidth * Number of DRAM controllers
// Number of DRAM controllers presently = Number of Cores
// m_dram_bandwidth is expressed in 'Bits per clock cycle'
// This DRAM model is not entirely correct.
// It sort of increases the queueing delay to a huge value if
// the arrival times of adjacent packets are spread over a large
// simulated time period
DramPerfModel::DramPerfModel(core_id_t core_id,
      SubsecondTime dram_access_cost,
      ComponentBandwidth dram_bandwidth,
      bool queue_model_enabled,
      String queue_model_type,
      UInt32 cache_block_size):
   m_queue_model(NULL),
   m_dram_access_cost(dram_access_cost),
   m_dram_bandwidth(dram_bandwidth),
   m_enabled(false),
   m_num_accesses(0),
   m_total_access_latency(SubsecondTime::Zero()),
   m_total_queueing_delay(SubsecondTime::Zero())
{
   if (queue_model_enabled)
   {
      m_queue_model = QueueModel::create("dram-queue", core_id, queue_model_type, m_dram_bandwidth.getRoundedLatency(8 * cache_block_size)); // bytes to bits
   }
   registerStatsMetric("dram", core_id, "total-access-latency", &m_total_access_latency);
   registerStatsMetric("dram", core_id, "total-queueing-delay", &m_total_queueing_delay);
}

DramPerfModel::~DramPerfModel()
{
   delete m_queue_model;
}

SubsecondTime
DramPerfModel::getAccessLatency(SubsecondTime pkt_time, UInt64 pkt_size, core_id_t requester)
{
   // pkt_size is in 'Bytes'
   // m_dram_bandwidth is in 'Bits per clock cycle'
   if ((!m_enabled) ||
         (requester >= (core_id_t) Config::getSingleton()->getApplicationCores()))
   {
      return SubsecondTime::Zero();
   }

   SubsecondTime processing_time = m_dram_bandwidth.getRoundedLatency(8 * pkt_size); // bytes to bits

   // Compute Queue Delay
   SubsecondTime queue_delay;
   if (m_queue_model)
   {
      queue_delay = m_queue_model->computeQueueDelay(pkt_time, processing_time, requester);
   }
   else
   {
      queue_delay = SubsecondTime::Zero();
   }

   SubsecondTime access_latency = queue_delay + processing_time + m_dram_access_cost;


   // Update Memory Counters
   m_num_accesses ++;
   m_total_access_latency += access_latency;
   m_total_queueing_delay += queue_delay;

   return access_latency;
}

void
DramPerfModel::outputSummary(std::ostream& out)
{
   out << "Dram Perf Model summary: " << std::endl;
   out << "    num dram accesses: " << m_num_accesses << std::endl;
   out << "    average dram access latency: " <<
      (m_num_accesses ? (m_total_access_latency / m_num_accesses) : SubsecondTime::Zero()) << std::endl;
   out << "    average dram queueing delay: " <<
      (m_num_accesses ? (m_total_queueing_delay / m_num_accesses) : SubsecondTime::Zero()) << std::endl;

   String queue_model_type = Sim()->getCfg()->getString("perf_model/dram/queue_model/type");
   if (m_queue_model && (queue_model_type == "history_list"))
   {
      out << "  Queue Model:" << std::endl;

      float queue_utilization = ((QueueModelHistoryList*) m_queue_model)->getQueueUtilization();
      float frac_requests_using_analytical_model = ((QueueModelHistoryList*) m_queue_model)->getFracRequestsUsingAnalyticalModel();
      out << "    Queue Utilization(\%): " << queue_utilization * 100 << std::endl;
      out << "    Analytical Model Used(\%): " << frac_requests_using_analytical_model * 100 << std::endl;
   }
}

void
DramPerfModel::dummyOutputSummary(std::ostream& out)
{
   out << "Dram Perf Model summary: " << std::endl;
   out << "    num dram accesses: NA" << std::endl;
   out << "    average dram access latency: NA" << std::endl;
   out << "    average dram queueing delay: NA" << std::endl;

   bool queue_model_enabled = Sim()->getCfg()->getBool("perf_model/dram/queue_model/enabled");
   String queue_model_type = Sim()->getCfg()->getString("perf_model/dram/queue_model/type");
   if (queue_model_enabled && (queue_model_type == "history_list"))
   {
      out << "  Queue Model:" << std::endl;
      out << "    Queue Utilization(\%): NA" << std::endl;
      out << "    Analytical Model Used(\%): NA" << std::endl;
   }
}