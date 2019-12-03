/**
*
* @file
*
* @brief  LaserCompact v5.2 support implementation
*
* @author vitamin.caig@gmail.com
*
**/

//local includes
#include "formats/image/container.h"
//common includes
#include <contract.h>
#include <make_ptr.h>
//library includes
#include <binary/format_factories.h>
#include <binary/input_stream.h>
#include <formats/image.h>
//text includes
#include <formats/text/image.h>

namespace Formats
{
namespace Image
{
  namespace LaserCompact52
  {
#ifdef USE_PRAGMA_PACK
#pragma pack(push,1)
#endif
    PACK_PRE struct Header
    {
      uint8_t Signature[5];
      uint16_t PackedSize;//starting from SizeCode, may be invalid
      uint8_t AdditionalSize;
    } PACK_POST;

    PACK_PRE struct SubHeader
    {
      uint8_t SizeCode;//0,1,2,8,9,16
      uint8_t ByteStream;
      uint8_t Bitstream;
    } PACK_POST;
#ifdef USE_PRAGMA_PACK
#pragma pack(pop)
#endif

    const std::size_t MIN_SIZE = 16;

    const std::size_t PIXELS_SIZE = 6144;
    const std::size_t ATTRS_SIZE = 768;

    class BitStream
    {
    public:
      explicit BitStream(Binary::View data)
        : Stream(data)
        , Bits()
        , Mask()
      {
      }

      uint8_t GetByte()
      {
        return Stream.ReadByte();
      }

      uint8_t GetBit()
      {
        if (0 == Mask)
        {
          Bits = GetByte();
          Mask = 0x80;
        }
        const uint8_t res = 0 != (Bits & Mask) ? 1 : 0;
        Mask >>= 1;
        return res;
      }

      uint8_t GetCode()
      {
        uint8_t res = 0xfe;
        for (uint_t i = 0; i < 3; ++i)
        {
          if (GetBit())
          {
            return res + 1;
          }
          res = 2 * res + GetBit();
        }
        return (2 * res + GetBit()) + 9;
      }

      uint8_t GetLen()
      {
        const uint8_t len = GetCode();
        if (len == 0x100 - 7)
        {
          return GetByte() - 1;
        }
        else if (len > 0x100 - 7)
        {
          return len - 1;
        }
        else
        {
          return len;
        }
      }

      std::size_t GetProcessedBytes() const
      {
        return Stream.GetPosition();
      }
    private:
      Binary::DataInputStream Stream;
      uint_t Bits;
      uint_t Mask;
    };

    class Container
    {
    public:
      explicit Container(Binary::View data)
        : Data(data)
      {
      }

      bool FastCheck() const
      {
        if (const auto* sub = GetCompressedData().As<SubHeader>())
        {
          return sub->SizeCode == 0
              || sub->SizeCode == 1
              || sub->SizeCode == 2
              || sub->SizeCode == 8
              || sub->SizeCode == 9
              || sub->SizeCode == 16
          ;
        }
        return false;
      }

      BitStream GetStream() const
      {
        return BitStream(GetCompressedData());
      }
    private:
      Binary::View GetCompressedData() const
      {
        if (const auto* hdr = Data.As<Header>())
        {
          return Data.SubView(sizeof(*hdr) + hdr->AdditionalSize);
        }
        return Binary::View(nullptr, 0);
      }
    private:
      const Binary::View Data;
    };

    class AddrTranslator
    {
    public:
      AddrTranslator(uint_t sizeCode)
        : AttrBase(256 * sizeCode)
        , ScrStart((AttrBase & 0x0300) << 3)
        , ScrLimit((AttrBase ^ 0x1800) & 0xfc00)
      {
      }

      std::size_t GetStart() const
      {
        return ScrStart;
      }

      std::size_t operator ()(std::size_t virtAddr) const
      {
        if (virtAddr < ScrLimit)
        {
          const std::size_t line = (virtAddr & 0x0007) << 8;
          const std::size_t row =  (virtAddr & 0x0038) << 2;
          const std::size_t col =  (virtAddr & 0x07c0) >> 6;
          return (virtAddr & 0x1800) | line | row | col;
        }
        else
        {
          return AttrBase + virtAddr;
        }
      }
    private:
      const std::size_t AttrBase;
      const std::size_t ScrStart;
      const std::size_t ScrLimit;
    };

    class DataDecoder
    {
    public:
      explicit DataDecoder(const Container& container)
        : IsValid(container.FastCheck())
        , Stream(container.GetStream())
      {
        if (IsValid)
        {
          IsValid = DecodeData();
        }
      }

      std::unique_ptr<Dump> GetResult()
      {
        return IsValid
          ? std::move(Result)
          : std::unique_ptr<Dump>();
      }

      std::size_t GetUsedSize() const
      {
        return Stream.GetProcessedBytes();
      }
    private:
      bool DecodeData()
      {
        try
        {
          Dump decoded(PIXELS_SIZE + ATTRS_SIZE);
          std::fill_n(&decoded[PIXELS_SIZE], ATTRS_SIZE, 7);

          const AddrTranslator translate(Stream.GetByte());

          std::size_t target = translate.GetStart();
          decoded.at(translate(target++)) = Stream.GetByte();
          for (;;)
          {
            if (Stream.GetBit())
            {
              decoded.at(translate(target++)) = Stream.GetByte();
            }
            else
            {
              uint8_t len = Stream.GetLen();
              if (0xff == len)
              {
                break;
              }
              uint16_t dist = Stream.GetCode() << 8;
              const int_t step = Stream.GetBit() ? -1 : +1;
              dist |= Stream.GetByte();

              len = -len;
              dist = -dist;
              if (dist > 768)
              {
                ++len;
              }
              uint16_t from = target - dist;
              do
              {
                decoded.at(translate(target++)) = decoded.at(translate(from));
                from += step;
              }
              while (--len > 0);
            }
          }
          Result.reset(new Dump());
          if (target <= PIXELS_SIZE)
          {
            decoded.resize(PIXELS_SIZE);
          }
          Result->swap(decoded);
          return true;
        }
        catch (const std::exception&)
        {
          return false;
        }
      }
    private:
      bool IsValid;
      BitStream Stream;
      std::unique_ptr<Dump> Result;
    };

    const std::string FORMAT(
      //Signature
      "'L'C'M'P'5"
    );
  }//namespace LaserCompact52

  class LaserCompact52Decoder : public Decoder
  {
  public:
    LaserCompact52Decoder()
      : Format(Binary::CreateFormat(LaserCompact52::FORMAT, LaserCompact52::MIN_SIZE))
    {
    }

    String GetDescription() const override
    {
      return Text::LASERCOMPACT52_DECODER_DESCRIPTION;
    }

    Binary::Format::Ptr GetFormat() const override
    {
      return Format;
    }

    Container::Ptr Decode(const Binary::Container& rawData) const override
    {
      if (!Format->Match(rawData))
      {
        return Container::Ptr();
      }
      const LaserCompact52::Container container(rawData);
      if (!container.FastCheck())
      {
        return Container::Ptr();
      }
      LaserCompact52::DataDecoder decoder(container);
      return CreateContainer(decoder.GetResult(), decoder.GetUsedSize());
    }
  private:
    const Binary::Format::Ptr Format;
  };

  Decoder::Ptr CreateLaserCompact52Decoder()
  {
    return MakePtr<LaserCompact52Decoder>();
  }
}//namespace Image
}//namespace Formats
