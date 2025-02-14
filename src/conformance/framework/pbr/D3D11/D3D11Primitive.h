// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include "D3D11Material.h"

#include <d3d11.h>
#include <d3d11_2.h>
#include <wrl/client.h>  // For Microsoft::WRL::ComPtr

#include <memory>
#include <vector>

namespace Pbr
{
    /// A primitive holds a vertex buffer, index buffer, and a pointer to a PBR material.
    struct D3D11Primitive final
    {
        using Collection = std::vector<D3D11Primitive>;

        D3D11Primitive() = delete;
        D3D11Primitive(UINT indexCount, Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer, Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer,
                       std::shared_ptr<D3D11Material> material, std::vector<NodeIndex_t> nodeIndices);
        D3D11Primitive(Pbr::D3D11Resources const& pbrResources, const Pbr::PrimitiveBuilder& primitiveBuilder,
                       const std::shared_ptr<D3D11Material>& material, bool updatableBuffers = false);

        void UpdateBuffers(_In_ ID3D11Device* device, _In_ ID3D11DeviceContext* context, const Pbr::PrimitiveBuilder& primitiveBuilder);

        /// Get the material for the primitive.
        const std::shared_ptr<D3D11Material>& GetMaterial() const
        {
            return m_material;
        }

        /// Replace the material for the primitive
        void SetMaterial(std::shared_ptr<D3D11Material> material)
        {
            m_material = std::move(material);
        }

        /// Get the nodes that the primitive represents
        const std::vector<NodeIndex_t>& GetNodes() const
        {
            return m_nodeIndices;
        }

    protected:
        friend class D3D11ModelInstance;
        void Render(_In_ ID3D11DeviceContext* context) const;
        D3D11Primitive Clone(Pbr::D3D11Resources const& pbrResources) const;

    private:
        UINT m_indexCount;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
        std::shared_ptr<D3D11Material> m_material;
        std::vector<NodeIndex_t> m_nodeIndices;
    };
}  // namespace Pbr
