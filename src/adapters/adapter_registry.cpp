// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-06] Adapter Registry (Chapter 3) / [API-06-05] Registry (Chapter 4)

#include "adapters/adapter_registry.h"

#include <algorithm>

namespace vgcpu {

AdapterRegistry& AdapterRegistry::Instance() {
    static AdapterRegistry instance;
    return instance;
}

void AdapterRegistry::Register(std::string id, std::string name, AdapterFactory factory) {
    adapters_.push_back({std::move(id), std::move(name), std::move(factory)});
}

std::vector<std::string> AdapterRegistry::GetAdapterIds() const {
    std::vector<std::string> ids;
    ids.reserve(adapters_.size());
    for (const auto& entry : adapters_) {
        ids.push_back(entry.id);
    }
    // Blueprint Reference: [REQ-29] Deterministic ordering (Chapter 2) / [ARCH-13-02c] SceneStats
    // ordering (Chapter 3)
    std::sort(ids.begin(), ids.end());
    return ids;
}

const std::vector<AdapterEntry>& AdapterRegistry::GetAdapters() const {
    return adapters_;
}

bool AdapterRegistry::HasAdapter(const std::string& id) const {
    return std::any_of(adapters_.begin(), adapters_.end(),
                       [&id](const AdapterEntry& e) { return e.id == id; });
}

std::unique_ptr<IBackendAdapter> AdapterRegistry::CreateAdapter(const std::string& id) const {
    for (const auto& entry : adapters_) {
        if (entry.id == id) {
            return entry.factory();
        }
    }
    return nullptr;
}

}  // namespace vgcpu
