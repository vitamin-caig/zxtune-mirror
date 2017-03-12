/**
 *
 * @file
 *
 * @brief Vfs root directory interface
 *
 * @author vitamin.caig@gmail.com
 *
 */

package app.zxtune.fs;

import android.net.Uri;

import java.io.IOException;

interface VfsRoot extends VfsDir {

  /**
   * Tries to resolve object by uri
   * @param uri Identifier to resolve
   * @return Found object or null otherwise
   */
  VfsObject resolve(Uri uri) throws IOException;
}
