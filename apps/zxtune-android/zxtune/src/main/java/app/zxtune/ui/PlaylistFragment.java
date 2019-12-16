/**
 * @file
 * @brief Playlist fragment component
 * @author vitamin.caig@gmail.com
 */

package app.zxtune.ui;

import android.content.Context;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.media.MediaMetadataCompat;
import android.support.v4.media.session.MediaControllerCompat;
import android.support.v4.media.session.PlaybackStateCompat;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.Observer;
import androidx.lifecycle.ViewModelProviders;
import androidx.recyclerview.selection.Selection;
import androidx.recyclerview.selection.SelectionPredicates;
import androidx.recyclerview.selection.SelectionTracker;
import androidx.recyclerview.selection.StorageStrategy;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import app.zxtune.Log;
import app.zxtune.R;
import app.zxtune.models.MediaSessionModel;
import app.zxtune.playlist.ProviderClient;
import app.zxtune.ui.playlist.PlaylistEntry;
import app.zxtune.ui.playlist.PlaylistViewAdapter;
import app.zxtune.ui.playlist.PlaylistViewModel;

public class PlaylistFragment extends Fragment {

  private static final String TAG = PlaylistFragment.class.getName();
  private ProviderClient ctrl;
  private RecyclerView listing;
  private ItemTouchHelper touchHelper;
  private View stub;
  private SelectionTracker<Long> selectionTracker;

  public static Fragment createInstance() {
    return new PlaylistFragment();
  }

  @Override
  public void onAttach(Context ctx) {
    super.onAttach(ctx);

    ctrl = new ProviderClient(ctx);
  }

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    setHasOptionsMenu(true);
  }

  @Override
  public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
    super.onCreateOptionsMenu(menu, inflater);

    inflater.inflate(R.menu.playlist, menu);
    final MenuItem sortItem = menu.findItem(R.id.action_sort);
    final SubMenu sortMenuRoot = sortItem.getSubMenu();
    for (ProviderClient.SortBy sortBy : ProviderClient.SortBy.values()) {
      for (ProviderClient.SortOrder sortOrder : ProviderClient.SortOrder.values()) {
        try {
          final MenuItem item = sortMenuRoot.add(getMenuTitle(sortBy));
          final ProviderClient.SortBy by = sortBy;
          final ProviderClient.SortOrder order = sortOrder;
          item.setOnMenuItemClickListener(new MenuItem.OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem item) {
              ctrl.sort(by, order);
              return true;
            }
          });
          item.setIcon(getMenuIcon(sortOrder));
        } catch (Exception e) {
          Log.w(TAG, e, "onCreateOptionsMenu");
        }
      }
    }
  }

  private static int getMenuTitle(ProviderClient.SortBy by) throws Exception {
    if (ProviderClient.SortBy.title.equals(by)) {
      return R.string.information_title;
    } else if (ProviderClient.SortBy.author.equals(by)) {
      return R.string.information_author;
    } else if (ProviderClient.SortBy.duration.equals(by)) {
      return R.string.statistics_duration;//TODO: extract
    } else {
      throw new Exception("Invalid sort order");
    }
  }

  private static int getMenuIcon(ProviderClient.SortOrder order) throws Exception {
    if (ProviderClient.SortOrder.asc.equals(order)) {
      return android.R.drawable.arrow_up_float;
    } else if (ProviderClient.SortOrder.desc.equals(order)) {
      return android.R.drawable.arrow_down_float;
    } else {
      throw new Exception("Invalid sor order");
    }
  }

  @Override
  public boolean onOptionsItemSelected(MenuItem item) {
    return processMenu(item.getItemId()) || super.onOptionsItemSelected(item);
  }

  @Override
  @Nullable
  public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, Bundle savedInstanceState) {
    return container != null ? inflater.inflate(R.layout.playlist, container, false) : null;
  }

  @Override
  public void onViewCreated(@NonNull View view, Bundle savedInstanceState) {
    super.onViewCreated(view, savedInstanceState);

    listing = view.findViewById(R.id.playlist_content);
    listing.setHasFixedSize(true);

    final PlaylistViewAdapter adapter = new PlaylistViewAdapter(new AdapterClient());
    listing.setAdapter(adapter);

    selectionTracker = new SelectionTracker.Builder<>("playlist_selection",
        listing,
        new PlaylistViewAdapter.KeyProvider(adapter),
        new PlaylistViewAdapter.DetailsLookup(listing),
        StorageStrategy.createLongStorage())
                           .withSelectionPredicate(SelectionPredicates.<Long>createSelectAnything())
                           .build();
    adapter.setSelection(selectionTracker.getSelection());

    SelectionUtils.install((AppCompatActivity) getActivity(), selectionTracker,
        new SelectionClient());

    if (savedInstanceState != null) {
      selectionTracker.onRestoreInstanceState(savedInstanceState);
    }

    touchHelper = new ItemTouchHelper(new TouchHelperCallback());
    touchHelper.attachToRecyclerView(listing);

    stub = view.findViewById(R.id.playlist_stub);

    final PlaylistViewModel playlistModel = PlaylistViewModel.of(this);
    playlistModel.getItems().observe(this, new Observer<List<PlaylistEntry>>() {
      @Override
      public void onChanged(@NonNull List<PlaylistEntry> entries) {
        adapter.submitList(entries);
        if (entries.isEmpty()) {
          listing.setVisibility(View.GONE);
          stub.setVisibility(View.VISIBLE);
        } else {
          listing.setVisibility(View.VISIBLE);
          stub.setVisibility(View.GONE);
        }
      }
    });
    final MediaSessionModel model = ViewModelProviders.of(getActivity()).get(MediaSessionModel.class);
    model.getState().observe(this, new Observer<PlaybackStateCompat>() {
      @Override
      public void onChanged(@Nullable PlaybackStateCompat state) {
        final boolean isPlaying = state != null && state.getState() == PlaybackStateCompat.STATE_PLAYING;
        adapter.setIsPlaying(isPlaying);
      }
    });
    model.getMetadata().observe(this, new Observer<MediaMetadataCompat>() {
      @Override
      public void onChanged(@Nullable MediaMetadataCompat metadata) {
        if (metadata != null) {
          final Uri uri = Uri.parse(metadata.getDescription().getMediaId());
          adapter.setNowPlaying(ProviderClient.findId(uri));
        }
      }
    });
  }

  @Override
  public void onSaveInstanceState(@NonNull Bundle outState) {
    selectionTracker.onSaveInstanceState(outState);
  }

  // Client for adapter
  class AdapterClient implements PlaylistViewAdapter.Client {
    @Override
    public void onClick(long id) {
      final MediaSessionModel model = ViewModelProviders.of(getActivity()).get(MediaSessionModel.class);
      final MediaControllerCompat ctrl = model.getMediaController().getValue();
      if (ctrl != null) {
        final Uri toPlay = ProviderClient.createUri(id);
        ctrl.getTransportControls().playFromUri(toPlay, null);
      }
    }

    @Override
    public void onDrag(@NonNull RecyclerView.ViewHolder holder) {
      if (!selectionTracker.hasSelection()) {
        touchHelper.startDrag(holder);
      }
    }
  }

  // Client for selection
  class SelectionClient implements SelectionUtils.Client {
    @NonNull
    @Override
    public String getTitle(int count) {
      return getResources().getQuantityString(R.plurals.tracks,
          count, count);
    }

    @NonNull
    @Override
    public List<Long> getAllItems() {
      final RecyclerView.Adapter adapter = listing.getAdapter();
      if (adapter == null) {
        return new ArrayList<>(0);
      }
      final int size = adapter.getItemCount();
      final ArrayList<Long> res = new ArrayList<>(size);
      for (int i = 0; i < size; ++i) {
        res.add(adapter.getItemId(i));
      }
      return res;
    }

    @Override
    public void fillMenu(MenuInflater inflater, Menu menu) {
      inflater.inflate(R.menu.playlist_items, menu);
    }

    @Override
    public boolean processMenu(int itemId) {
      return PlaylistFragment.this.processMenu(itemId);
    }
  }

  private boolean processMenu(int itemId) {
    switch (itemId) {
      case R.id.action_clear:
        ctrl.deleteAll();
        break;
      case R.id.action_delete:
        ctrl.delete(getSelection());
        break;
      case R.id.action_save:
        savePlaylist(getSelection());
        break;
      case R.id.action_statistics:
        showStatistics(getSelection());
        break;
      default:
        return false;
    }
    return true;
  }

  private long[] getSelection() {
    final Selection<Long> selection = selectionTracker.getSelection();
    if (selection == null || selection.size() == 0) {
      return null;
    }
    final long[] result = new long[selection.size()];
    final Iterator<Long> iter = selection.iterator();
    for (int idx = 0; idx < result.length; ++idx) {
      result[idx] = iter.next();
    }
    return result;
  }

  private void savePlaylist(@Nullable long[] ids) {
    PlaylistSaveFragment.createInstance(ids).show(getFragmentManager(), "save");
  }

  private void showStatistics(@Nullable long[] ids) {
    PlaylistStatisticsFragment.createInstance(ids).show(getFragmentManager(), "statistics");
  }

  private class TouchHelperCallback extends ItemTouchHelper.SimpleCallback {

    long draggedItem = -1;
    int dragDelta = 0;

    TouchHelperCallback() {
      super(ItemTouchHelper.UP | ItemTouchHelper.DOWN, 0);
    }

    @Override
    public boolean isLongPressDragEnabled() {
      return false;
    }

    @Override
    public boolean isItemViewSwipeEnabled() {
      return false;
    }

    @Override
    public void onSelectedChanged(RecyclerView.ViewHolder viewHolder, int actionState) {
      if (actionState != ItemTouchHelper.ACTION_STATE_IDLE) {
        viewHolder.itemView.setBackgroundColor(Color.BLACK);
      }
      super.onSelectedChanged(viewHolder, actionState);
    }

    @Override
    public void clearView(@NonNull RecyclerView view, @NonNull RecyclerView.ViewHolder viewHolder) {
      super.clearView(view, viewHolder);
      viewHolder.itemView.setBackgroundColor(Color.TRANSPARENT);

      if (draggedItem != -1 && dragDelta != 0) {
        ctrl.move(draggedItem, dragDelta);
      }
      draggedItem = -1;
      dragDelta = 0;
    }

    @Override
    public boolean onMove(@NonNull RecyclerView recyclerView, RecyclerView.ViewHolder source,
                          RecyclerView.ViewHolder target) {
      final int srcPos = source.getAdapterPosition();
      final int tgtPos = target.getAdapterPosition();
      if (draggedItem == -1) {
        draggedItem = source.getItemId();
      }
      dragDelta += tgtPos - srcPos;
      final PlaylistViewAdapter adapter = ((PlaylistViewAdapter) recyclerView.getAdapter());
      if (adapter != null) {
        adapter.onItemMove(srcPos, tgtPos);
      }
      return true;
    }

    @Override
    public void onSwiped(@NonNull RecyclerView.ViewHolder viewHolder, int direction) {
    }
  }
}
