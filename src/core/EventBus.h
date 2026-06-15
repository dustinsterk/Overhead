#pragma once
#include <functional>
#include <vector>
#include "Ids.h"

// core/EventBus — provider -> page/director notifications (spec §4).
//
// Deliberately tiny: providers call publish(id) when their model changes;
// subscribers receive the id and pull the fresh model from the provider. We
// pass only the id (not the model) to avoid type erasure on a heterogeneous set
// of models. All publish/deliver happens on the UI thread (network results are
// marshalled back via NetClient::poll), so handlers need no locking.
class EventBus {
public:
  using Handler = std::function<void(ProviderId)>;

  void subscribe(Handler h) { _subs.push_back(std::move(h)); }
  void publish(ProviderId id) { for (auto& h : _subs) h(id); }

private:
  std::vector<Handler> _subs;
};
