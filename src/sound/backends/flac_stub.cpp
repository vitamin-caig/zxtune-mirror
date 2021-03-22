/**
 *
 * @file
 *
 * @brief  FLAC backend stub
 *
 * @author vitamin.caig@gmail.com
 *
 **/

// local includes
#include "sound/backends/flac.h"
#include "sound/backends/storage.h"
// library includes
#include <l10n/api.h>
#include <sound/backend_attrs.h>

namespace Sound
{
  void RegisterFlacBackend(BackendsStorage& storage)
  {
    storage.Register(Flac::BACKEND_ID, L10n::translate("FLAC support backend."), CAP_TYPE_FILE);
  }
}  // namespace Sound
