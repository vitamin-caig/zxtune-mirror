/**
 * @file
 * @brief
 * @version $Id:$
 * @author
 */
package app.zxtune.playback;

public final class StubIterator extends Iterator {

  private StubIterator() {
  }

  @Override
  public PlayableItem getItem() {
    throw new IllegalStateException("Should not be called");
  }

  @Override
  public boolean next() {
    return false;
  }

  @Override
  public boolean prev() {
    return false;
  }


  public static Iterator instance() {
    return Holder.INSTANCE;
  }

  //onDemand holder idiom
  private static class Holder {
    public static final Iterator INSTANCE = new StubIterator();
  }  
}