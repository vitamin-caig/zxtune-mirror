/**
 *
 * @file
 *
 * @brief  Names set helper
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// library includes
#include <parameters/types.h>
// std includes
#include <set>

namespace Parameters
{
  class NamesSet
  {
  public:
    bool Insert(StringView name)
    {
      const auto lower = Storage.lower_bound(name);
      if (lower != Storage.end() && *lower == name)
      {
        return false;
      }
      Storage.insert(lower, name.to_string());
      return true;
    }

    bool Has(StringView name) const
    {
      return Storage.end() != Storage.find(name);
    }

    bool Erase(StringView name)
    {
      const auto it = Storage.find(name);
      return it != Storage.end() ? (Storage.erase(it), true) : false;
    }

  private:
    std::set<String, std::less<>> Storage;
  };
}  // namespace Parameters
