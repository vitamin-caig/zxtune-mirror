/**
 *
 * @file
 *
 * @brief  Module renderer interface
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// library includes
#include <module/state.h>
#include <sound/chunk.h>

namespace Sound
{
  struct LoopParameters;
}

namespace Module
{
  //! @brief %Module player interface
  class Renderer
  {
  public:
    //! @brief Generic pointer type
    typedef std::shared_ptr<Renderer> Ptr;

    virtual ~Renderer() = default;

    //! @brief Current tracking status
    virtual State::Ptr GetState() const = 0;

    //! @brief Rendering single frame and modifying internal state
    //! @return empty chunk if there's no more data to render
    virtual Sound::Chunk Render(const Sound::LoopParameters& looped) = 0;

    //! @brief Performing reset to initial state
    virtual void Reset() = 0;

    //! @brief Seeking
    //! @param position to seek
    //! @note Seeking out of range is safe, but state will be MODULE_PLAYING untill next RenderFrame call happends.
    //! @note It produces only the flush
    virtual void SetPosition(Time::AtMillisecond position) = 0;
  };
}  // namespace Module
