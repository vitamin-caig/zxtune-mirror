/*
Abstract:
  Analysis scanner implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//common includes
#include <iterator.h>
//library includes
#include <analysis/scanner.h>
#include <debug/log.h>
//std includes
#include <deque>
#include <list>
//boost includes
#include <boost/make_shared.hpp>

namespace
{
  const Debug::Stream Dbg("Analysis::Scanner");
}

namespace Analysis
{
  using namespace Formats;

  template<class DecoderPtrType>
  class DecodersQueue
  {
  public:
    void Add(std::size_t offset, DecoderPtrType decoder)
    {
      Storage.push_back(PositionAndDecoder(offset, decoder));
      Storage.sort();
    }

    bool IsEmpty() const
    {
      return Storage.empty();
    }

    std::size_t GetPosition() const
    {
      return Storage.front().Position;
    }

    DecoderPtrType FetchDecoder()
    {
      const DecoderPtrType result = Storage.front().Decoder;
      Storage.pop_front();
      return result;
    }

    void Reset()
    {
      StorageType other;
      for (typename StorageType::iterator it = Storage.begin(), lim = Storage.end(); it != lim; ++it)
      {
        other.push_back(PositionAndDecoder(0, it->Decoder));
      }
      Storage.swap(other);
    }
  private:
    struct PositionAndDecoder
    {
      const std::size_t Position;
      const DecoderPtrType Decoder;

      PositionAndDecoder(std::size_t pos, DecoderPtrType decoder)
        : Position(pos)
        , Decoder(decoder)
      {
      }

      bool operator < (const PositionAndDecoder& rh) const
      {
        return Position < rh.Position;
      }
    };
    typedef std::list<PositionAndDecoder> StorageType;
    StorageType Storage;
  };

  class ScanningContainer : public Binary::Container
  {
  public:
    explicit ScanningContainer(const Binary::Container& delegate)
      : Delegate(delegate)
      , Limit(Delegate.Size())
      , Content(static_cast<const uint8_t*>(Delegate.Start()))
      , Offset(0)
    {
    }

    std::size_t GetOffset() const
    {
      return Offset;
    }

    void SetOffset(std::size_t newOffset)
    {
      Offset = std::max(Offset, newOffset);
    }

    void Advance(std::size_t delta)
    {
      Offset += delta;
    }

    bool IsEmpty() const
    {
      return Limit == Offset;
    }

    void SetEmpty()
    {
      Offset = Limit;
    }

    //Binary::Container
    virtual const void* Start() const
    {
      return Content + Offset;
    }

    virtual std::size_t Size() const
    {
      return Limit - Offset;
    }

    virtual Binary::Container::Ptr GetSubcontainer(std::size_t offset, std::size_t size) const
    {
      return Delegate.GetSubcontainer(Offset + offset, size);
    }
  private:
    const Binary::Container& Delegate;
    const std::size_t Limit;
    const uint8_t* const Content;
    std::size_t Offset;
  };

  template<class Traits>
  class TypedScanner
  {
  public:
    TypedScanner(const typename Traits::DecodersList& storage, std::size_t base, const Binary::Container& data)
      : Unprocessed(storage.begin(), storage.end())
      , Base(base)
      , Window(data)
      , Unrecognized(data)
    {
    }

    void Scan(Scanner::Target& target)
    {
      Scheduled.Reset();
      Window.SetOffset(0);
      Unrecognized.SetOffset(0);
      ScanFromStart(target);
      while (!IsFinished())
      {
        ScanFromLookaheads(target);
      }
      FlushUnrecognized(target);
    }
  private:
    bool IsFinished() const
    {
      return Window.IsEmpty();
    }

    void ScanFromStart(Scanner::Target& target)
    {
      while (!IsFinished() && Unprocessed)
      {
        ProcessDecoder(*Unprocessed, target);
        ++Unprocessed;
      }
    }

    void ScanFromLookaheads(Scanner::Target& target)
    {
      while (!IsFinished())
      {
        if (Scheduled.IsEmpty())
        {
          Window.SetEmpty();
          break;
        }
        Window.SetOffset(Scheduled.GetPosition());
        ProcessDecoder(Scheduled.FetchDecoder(), target);
      }
    }

    void ProcessDecoder(typename Traits::Decoder::Ptr decoder, Scanner::Target& target)
    {
      Dbg("Trying %1% at %2%+%3%", decoder->GetDescription(), Base, Window.GetOffset());
      if (const typename Traits::Container::Ptr result = decoder->Decode(Window))
      {
        const std::size_t used = Traits::GetUsedSize(*result);
        Dbg(" found %1% bytes", used);
        FlushUnrecognized(target);
        target.Apply(*decoder, Base + Window.GetOffset(), result);
        Schedule(decoder, used);
        Window.Advance(used);
        Unrecognized.SetOffset(Window.GetOffset());
      }
      else
      {
        Schedule(decoder, decoder->GetFormat()->NextMatchOffset(Window));
      }
    }

    void FlushUnrecognized(Scanner::Target& target) const
    {
      const std::size_t till = Window.GetOffset();
      const std::size_t unmatchedAt = Unrecognized.GetOffset();
      if (till > unmatchedAt)
      {
        Dbg("Unrecognized %1%+(%2%..%3%)", Base, unmatchedAt, till);
        target.Apply(Base + unmatchedAt, Unrecognized.GetSubcontainer(0, till - unmatchedAt));
      }
    }

    void Schedule(typename Traits::Decoder::Ptr decoder, std::size_t delta)
    {
      if (delta != Window.Size())
      {
        const std::size_t nextPos = Window.GetOffset() + delta;
        Dbg(" skip %1% bytes (check at %2%)", delta, nextPos);
        Scheduled.Add(nextPos, decoder);
      }
      else
      {
        Dbg(" disable for future scans");
      }
    }
  private:
    RangeIterator<typename Traits::DecodersList::const_iterator> Unprocessed;
    DecodersQueue<typename Traits::Decoder::Ptr> Scheduled;
    const std::size_t Base;
    ScanningContainer Window;
    ScanningContainer Unrecognized;
  };

  struct ArchivedDataTraits
  {
    typedef Archived::Decoder Decoder;
    typedef Archived::Container Container;
    typedef std::deque<Decoder::Ptr> DecodersList;

    static std::size_t GetUsedSize(const Container& container)
    {
      return container.Size();
    }
  };

  struct PackedDataTraits
  {
    typedef Packed::Decoder Decoder;
    typedef Packed::Container Container;
    typedef std::deque<Decoder::Ptr> DecodersList;

    static std::size_t GetUsedSize(const Container& container)
    {
      return container.PackedSize();
    }
  };

  struct ChiptuneDataTraits
  {
    typedef Chiptune::Decoder Decoder;
    typedef Chiptune::Container Container;
    typedef std::deque<Decoder::Ptr> DecodersList;

    static std::size_t GetUsedSize(const Container& container)
    {
      return container.Size();
    }
  };

  struct DecodersSet
  {
    ArchivedDataTraits::DecodersList Archived;
    PackedDataTraits::DecodersList Packed;
    ChiptuneDataTraits::DecodersList Chiptune;
  };

  template<class Traits>
  class DecodeUnrecognizedTarget : public Scanner::Target
  {
  public:
    DecodeUnrecognizedTarget(const typename Traits::DecodersList& decoders, Scanner::Target& recognized, Scanner::Target& unrecognized)
      : Decoders(decoders)
      , Recognized(recognized)
      , Unrecognized(unrecognized)
    {
    }

    virtual void Apply(const Archived::Decoder& decoder, std::size_t offset, Archived::Container::Ptr data)
    {
      Recognized.Apply(decoder, offset, data);
    }

    virtual void Apply(const Packed::Decoder& decoder, std::size_t offset, Packed::Container::Ptr data)
    {
      Recognized.Apply(decoder, offset, data);
    }

    virtual void Apply(const Chiptune::Decoder& decoder, std::size_t offset, Chiptune::Container::Ptr data)
    {
      Recognized.Apply(decoder, offset, data);
    }

    virtual void Apply(std::size_t offset, Binary::Container::Ptr data)
    {
      TypedScanner<Traits> scanner(Decoders, offset, *data);
      scanner.Scan(Unrecognized);
    }

    Scanner::Target& GetUnrecognizedTarget()
    {
      return Decoders.empty()
        ? Unrecognized
        : *this;
    }
  private:
    const typename Traits::DecodersList& Decoders;
    Scanner::Target& Recognized; 
    Scanner::Target& Unrecognized;
  };

  class LinearScanner : public Scanner
  {
  public:
    virtual void AddDecoder(Archived::Decoder::Ptr decoder)
    {
      Decoders.Archived.push_back(decoder);
    }

    virtual void AddDecoder(Packed::Decoder::Ptr decoder)
    {
      Decoders.Packed.push_back(decoder);
    }

    virtual void AddDecoder(Chiptune::Decoder::Ptr decoder)
    {
      Decoders.Chiptune.push_back(decoder);
    }

    virtual void Scan(Binary::Container::Ptr data, Target& target) const
    {
      //order: Archive -> Packed -> Chiptune -> unrecognized with possible skipping
      DecodeUnrecognizedTarget<ChiptuneDataTraits> chiptune(Decoders.Chiptune, target, target);
      DecodeUnrecognizedTarget<PackedDataTraits> packed(Decoders.Packed, target, chiptune.GetUnrecognizedTarget());
      DecodeUnrecognizedTarget<ArchivedDataTraits> archived(Decoders.Archived, target, packed.GetUnrecognizedTarget());

      archived.Apply(0, data);
    }
  private:
    DecodersSet Decoders;
  };

  Scanner::RWPtr CreateScanner()
  {
    return boost::make_shared<LinearScanner>();
  }
}
