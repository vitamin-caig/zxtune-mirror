package app.zxtune.core;

import android.support.annotation.NonNull;

/**
 * Player interface
 */
public interface Player extends PropertiesAccessor, PropertiesModifier {

  /**
   * @return Index of next rendered frame
   */
  int getPosition() throws Exception;

  /**
   * @param bands  Array of bands to store
   * @param levels Array of levels to store
   * @return Count of actually stored entries
   */
  int analyze(@NonNull int bands[], @NonNull int levels[]) throws Exception;

  /**
   * Render next result.length bytes of sound data
   *
   * @param result Buffer to put data
   * @return Is there more data to render
   */
  boolean render(@NonNull short[] result) throws Exception;

  /**
   * @param pos Index of next rendered frame
   */
  void setPosition(int pos) throws Exception;
}
