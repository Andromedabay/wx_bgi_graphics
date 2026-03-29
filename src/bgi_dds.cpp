/**
 * @file bgi_dds.cpp
 * @brief DdsScene container implementation.
 */

#include "bgi_dds.h"

#include <algorithm>

namespace bgi
{

// ---------------------------------------------------------------------------
// DdsScene::appendWithId
// ---------------------------------------------------------------------------

std::shared_ptr<DdsObject> DdsScene::appendWithId(std::shared_ptr<DdsObject> obj)
{
    // Caller has already set obj->id; just insert.
    order.push_back(obj->id);
    index[obj->id] = obj;
    // Keep nextId ahead of any restored numeric id.
    try
    {
        uint64_t numId = std::stoull(obj->id);
        if (numId >= nextId)
            nextId = numId + 1;
    }
    catch (...) {}
    return obj;
}

// ---------------------------------------------------------------------------
// DdsScene::append
// ---------------------------------------------------------------------------

std::shared_ptr<DdsObject> DdsScene::append(std::shared_ptr<DdsObject> obj)
{
    obj->id = genId();
    order.push_back(obj->id);
    index[obj->id] = obj;
    return obj;
}

// ---------------------------------------------------------------------------
// DdsScene::findById
// ---------------------------------------------------------------------------

std::shared_ptr<DdsObject> DdsScene::findById(const std::string &id) const
{
    auto it = index.find(id);
    if (it == index.end() || it->second->deleted)
        return nullptr;
    return it->second;
}

// ---------------------------------------------------------------------------
// DdsScene::findByLabel
// ---------------------------------------------------------------------------

std::shared_ptr<DdsObject> DdsScene::findByLabel(const std::string &label) const
{
    if (label.empty())
        return nullptr;
    for (const auto &id : order)
    {
        auto it = index.find(id);
        if (it != index.end() && !it->second->deleted && it->second->label == label)
            return it->second;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// DdsScene::findIdByLabel
// ---------------------------------------------------------------------------

std::string DdsScene::findIdByLabel(const std::string &label) const
{
    auto obj = findByLabel(label);
    return obj ? obj->id : std::string{};
}

// ---------------------------------------------------------------------------
// DdsScene::remove (soft-delete)
// ---------------------------------------------------------------------------

bool DdsScene::remove(const std::string &id)
{
    auto it = index.find(id);
    if (it == index.end())
        return false;
    it->second->deleted = true;
    return true;
}

// ---------------------------------------------------------------------------
// DdsScene::clearDrawingObjects
// ---------------------------------------------------------------------------

void DdsScene::clearDrawingObjects()
{
    // Soft-delete all non-config objects.
    for (const auto &id : order)
    {
        auto it = index.find(id);
        if (it == index.end())
            continue;
        const auto t = it->second->type;
        if (t != DdsObjectType::Camera &&
            t != DdsObjectType::Ucs &&
            t != DdsObjectType::WorldExtentsObj)
        {
            it->second->deleted = true;
        }
    }
    compact();
}

// ---------------------------------------------------------------------------
// DdsScene::clearAll
// ---------------------------------------------------------------------------

void DdsScene::clearAll()
{
    order.clear();
    index.clear();
    nextId = 1;
}

// ---------------------------------------------------------------------------
// DdsScene::compact
// ---------------------------------------------------------------------------

void DdsScene::compact()
{
    // Remove soft-deleted entries from both collections.
    order.erase(
        std::remove_if(order.begin(), order.end(),
                       [this](const std::string &id) {
                           auto it = index.find(id);
                           return it == index.end() || it->second->deleted;
                       }),
        order.end());

    for (auto it = index.begin(); it != index.end();)
    {
        if (it->second->deleted)
            it = index.erase(it);
        else
            ++it;
    }
}

// ---------------------------------------------------------------------------
// DdsScene::count
// ---------------------------------------------------------------------------

int DdsScene::count() const
{
    int n = 0;
    for (const auto &id : order)
    {
        auto it = index.find(id);
        if (it != index.end() && !it->second->deleted)
            ++n;
    }
    return n;
}

// ---------------------------------------------------------------------------
// DdsScene::countDrawing
// ---------------------------------------------------------------------------

int DdsScene::countDrawing() const
{
    int n = 0;
    for (const auto &id : order)
    {
        auto it = index.find(id);
        if (it == index.end() || it->second->deleted)
            continue;
        const auto t = it->second->type;
        if (t != DdsObjectType::Camera &&
            t != DdsObjectType::Ucs &&
            t != DdsObjectType::WorldExtentsObj)
        {
            ++n;
        }
    }
    return n;
}

// ---------------------------------------------------------------------------
// DdsScene::idAt
// ---------------------------------------------------------------------------

std::string DdsScene::idAt(int idx) const
{
    int n = 0;
    for (const auto &id : order)
    {
        auto it = index.find(id);
        if (it != index.end() && !it->second->deleted)
        {
            if (n == idx)
                return id;
            ++n;
        }
    }
    return {};
}

} // namespace bgi
