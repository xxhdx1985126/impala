// Copyright 2012 Cloudera Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#ifndef SPARROW_SIMPLE_SCHEDULER_H
#define SPARROW_SIMPLE_SCHEDULER_H

#include <vector>
#include <string>
#include <list>
#include <boost/unordered_map.hpp>
#include <boost/thread/mutex.hpp>

#include "common/status.h"
#include "statestore/scheduler.h"
#include "statestore/subscription-manager.h"
#include "statestore/util.h"
#include "statestore/state-store.h"
#include "util/metrics.h"
#include "gen-cpp/Types_types.h"  // for TNetworkAddress

namespace impala {

// Performs simple scheduling by matching between a list of hosts configured
// either from the state-store, or from a static list of addresses, and a list
// of target data locations.
class SimpleScheduler : public Scheduler {
 public:
  // Initialize with a subscription manager that we can register with for updates to the
  // set of available backends.
  SimpleScheduler(SubscriptionManager* subscription_manager,
      const ServiceId& backend_service_id, impala::Metrics* metrics);

  // Initialize with a list of <host:port> pairs in 'static' mode - i.e. the set of
  // backends is fixed and will not be updated.
  SimpleScheduler(const std::vector<impala::TNetworkAddress>& backends,
      impala::Metrics* metrics);

  virtual ~SimpleScheduler();

  // Returns a list of backends such that the impalad at hostports[i] should be used to
  // read data from data_locations[i].
  // For each data_location, we choose a backend whose host matches the data_location in
  // a round robin fashion and insert it into hostports.
  // If no match is found for a data location, assign the data location in round-robin
  // order to any of the backends.
  // If the set of available hosts is updated between calls, round-robin state is reset.
  virtual impala::Status GetHosts(const HostList& data_locations, HostList* hostports);

  // Return a backend such that the impalad at hostport should be used to read data
  // from the given data_loation
  virtual impala::Status GetHost(const TNetworkAddress& data_location,
      TNetworkAddress* hostport);

  virtual void GetAllKnownHosts(HostList* hostports);

  virtual bool HasLocalHost(const TNetworkAddress& data_location) {
    HostLocalityMap::iterator entry = host_map_.find(data_location.hostname);
    return (entry != host_map_.end());
  }

  // Registers with the subscription manager if required
  virtual impala::Status Init();

  // Unregister with the subscription manager
  virtual void Close();

 private:
  // Map from IP to a list of addresses of Impala daemons that are
  // local for that address. Keys in this map must not be hostnames,
  // since they are compared to the block location IP addresses
  // returned by the namenode.
  typedef boost::unordered_map<std::string, std::list<TNetworkAddress> > HostLocalityMap;
  HostLocalityMap host_map_;

  // Metrics subsystem access
  impala::Metrics* metrics_;

  // Protects access to host_map_, which may be updated asynchronously with respect to
  // reads. Also protects the locality counters, which are updated in GetHosts.
  boost::mutex host_map_lock_;

  // round robin entry in HostLocalityMap for non-local host assignment
  HostLocalityMap::iterator next_nonlocal_host_entry_;

  // Pointer to a subscription manager (which we do not own) which is used to register
  // for dynamic updates to the set of available backends. May be NULL if the set of
  // backends is fixed.
  SubscriptionManager* subscription_manager_;

  // UpdateCallback to use for registering a subscription with the subscription manager.
  SubscriptionManager::UpdateCallback callback_;

  // Subscription handle, used to unregister with subscription manager
  SubscriptionId subscription_id_;

  // Service identifier to subscribe to for backend membership information
  ServiceId backend_service_id_;

  // Locality metrics
  impala::Metrics::IntMetric* total_assignments_;
  impala::Metrics::IntMetric* total_local_assignments_;

  // Initialisation metric
  impala::Metrics::BooleanMetric* initialised_;

  // Called asynchronously when an update is received from the subscription manager
  void UpdateMembership(const ServiceStateMap& service_state);
};

}

#endif
