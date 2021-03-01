package app.zxtune.core.jni;

import androidx.annotation.Nullable;

import java.nio.ByteBuffer;

import app.zxtune.core.Module;
import app.zxtune.core.PropertiesContainer;
import app.zxtune.core.ResolvingException;
import app.zxtune.utils.ProgressCallback;

public class JniApi {

  static {
    System.loadLibrary("zxtune");
  }

  public static native Module loadModule(ByteBuffer data, String subPath) throws ResolvingException;

  public static native void detectModules(ByteBuffer data, DetectCallback callback, @Nullable ProgressCallback progress);

  public static native void enumeratePlugins(Plugins.Visitor visitor);

  public static PropertiesContainer getOptions() {
    return JniOptions.instance();
  }
}
