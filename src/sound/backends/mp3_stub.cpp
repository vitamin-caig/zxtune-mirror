/**
 *
 * @file
 *
 * @brief  MP3 backend stub
 *
 * @author vitamin.caig@gmail.com
 *
 **/

// local includes
#include "sound/backends/mp3.h"
#include "sound/backends/storage.h"
// library includes
#include <l10n/api.h>
#include <sound/backend_attrs.h>

namespace Sound
{
  void RegisterMp3Backend(BackendsStorage& storage)
  {
    storage.Register(Mp3::BACKEND_ID, L10n::translate("MP3 support backend"), CAP_TYPE_FILE);
  }
}  // namespace Sound
