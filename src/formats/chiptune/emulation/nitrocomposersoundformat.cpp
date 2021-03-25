/**
 *
 * @file
 *
 * @brief  NCSF parser implementation
 *
 * @author liushuyu011@gmail.com
 *
 **/

// local includes
#include "formats/chiptune/emulation/nitrocomposersoundformat.h"
// common includes
#include <byteorder.h>
#include <make_ptr.h>
// library includes
#include <binary/compression/zlib_container.h>
#include <binary/format_factories.h>
#include <binary/input_stream.h>
// text includes
#include <formats/text/chiptune.h>

namespace Formats::Chiptune
{
  namespace NitroComposerSoundFormat
  {
    typedef std::array<uint8_t, 4> SignatureType;
    const SignatureType SAVESTATE_SIGNATURE = {{'S', 'A', 'V', 'E'}};

    void ParseRom(Binary::View data, Builder& target)
    {
      Binary::DataInputStream stream(data);
      stream.Seek(8);
      const auto size = stream.ReadLE<uint32_t>();
      stream.Seek(0);
      target.SetChunk(0, stream.ReadData(size));
      Require(0 == stream.GetRestSize());
    }

    uint32_t ParseState(Binary::View data)
    {
      Binary::DataInputStream stream(data);
      if (data.Size() >= 4)
      {
        return stream.ReadLE<uint32_t>();
      }
      return 0;
    }

    const std::string FORMAT(
        "'P'S'F"
        "25");

    class Decoder : public Formats::Chiptune::Decoder
    {
    public:
      Decoder()
        : Format(Binary::CreateMatchOnlyFormat(FORMAT))
      {}

      String GetDescription() const override
      {
        return Text::NITROCOMPOSERSOUNDFORMAT_DECODER_DESCRIPTION;
      }

      Binary::Format::Ptr GetFormat() const override
      {
        return Format;
      }

      bool Check(Binary::View rawData) const override
      {
        return Format->Match(rawData);
      }

      Formats::Chiptune::Container::Ptr Decode(const Binary::Container& /*rawData*/) const override
      {
        return Formats::Chiptune::Container::Ptr();  // TODO
      }

    private:
      const Binary::Format::Ptr Format;
    };
  }  // namespace NitroComposerSoundFormat

  Decoder::Ptr CreateNCSFDecoder()
  {
    return MakePtr<NitroComposerSoundFormat::Decoder>();
  }
}  // namespace Formats::Chiptune