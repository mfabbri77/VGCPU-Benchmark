// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: Chapter 7, §7.2.5 — Adapter registration

#pragma once

#include "adapters/adapter_interface.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace vgcpu {

/// Factory function type for creating adapters.
using AdapterFactory = std::function<std::unique_ptr<IBackendAdapter>()>;

/// Adapter registry entry.
struct AdapterEntry {
    std::string id;
    std::string name;
    AdapterFactory factory;
};

/// Registry of available backend adapters.
/// Blueprint Reference: Chapter 7, §7.2.5 — Adapter registry/catalog
class AdapterRegistry {
   public:
    /// Get the singleton instance.
    static AdapterRegistry& Instance();

    /// Register an adapter with the registry.
    void Register(std::string id, std::string name, AdapterFactory factory);

    /// Get list of registered adapter IDs.
    [[nodiscard]] std::vector<std::string> GetAdapterIds() const;

    /// Get list of all adapter entries.
    [[nodiscard]] const std::vector<AdapterEntry>& GetAdapters() const;

    /// Check if an adapter is registered.
    [[nodiscard]] bool HasAdapter(const std::string& id) const;

    /// Create an adapter instance by ID.
    /// @return nullptr if adapter not found.
    [[nodiscard]] std::unique_ptr<IBackendAdapter> CreateAdapter(const std::string& id) const;

   private:
    AdapterRegistry() = default;
    std::vector<AdapterEntry> adapters_;
};

/// Helper macro for registering adapters at static initialization time.
#define VGCPU_REGISTER_ADAPTER(id, name, factory_func)                 \
    static bool vgcpu_register_##id = []() {                           \
        AdapterRegistry::Instance().Register(#id, name, factory_func); \
        return true;                                                   \
    }()

}  // namespace vgcpu
