/***************************************************************************
 *
 * Project         _____    __   ____   _      _
 *                (  _  )  /__\ (_  _)_| |_  _| |_
 *                 )(_)(  /(__)\  )( (_   _)(_   _)
 *                (_____)(__)(__)(__)  |_|    |_|
 *
 *
 * Copyright 2018-present, Leonid Stryzhevskyi <lganzzzo@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************************/

#include "Beautifier.hpp"

#include "oatpp/core/data/stream/BufferStream.hpp"

namespace oatpp { namespace parser { namespace json {

Beautifier::Beautifier(ConsistentOutputStream* outputStream, const oatpp::String& indent, const oatpp::String& newLine)
  : m_outputStream(outputStream)
  , m_indent(indent)
  , m_newLine(newLine)
  , m_level(0)
  , m_wantIndent(false)
  , m_isCharEscaped(false)
  , m_isInString(false)
{}

void Beautifier::writeIndent(ConsistentOutputStream* outputStream) {
  outputStream->writeSimple(m_newLine->getData(), m_newLine->getSize());
  for(v_int32 i = 0; i < m_level; i ++ ) {
    outputStream->writeSimple(m_indent->getData(), m_indent->getSize());
  }
}

v_io_size Beautifier::write(const void *data, v_buff_size count, async::Action& action) {

  oatpp::data::stream::BufferOutputStream buffer;

  for(v_buff_size i = 0; i < count; i ++) {

    v_char8 c = ((p_char8) data) [i];

    if(m_isCharEscaped) {
      m_isCharEscaped = false;
      buffer.writeCharSimple(c);
      continue;
    }

    switch(c) {

      case '\\': {
        m_isCharEscaped = true;
        buffer.writeCharSimple('\\');
        break;
      }

      case '{': {
        if(m_wantIndent) {
          writeIndent(&buffer);
        }
        m_level ++;
        m_wantIndent = true;
        buffer.writeCharSimple('{');
        break;
      }

      case '}': {
        m_level --;
        if(!m_wantIndent) {
          writeIndent(&buffer);
        }
        buffer.writeCharSimple('}');
        m_wantIndent = false;
        break;
      }

      case '[': {
        if(m_wantIndent) {
          writeIndent(&buffer);
        }
        m_level ++;
        m_wantIndent = true;
        buffer.writeCharSimple('[');
        break;
      }

      case ']': {
        m_level --;
        if(!m_wantIndent) {
          writeIndent(&buffer);
        }
        buffer.writeCharSimple(']');
        m_wantIndent = false;
        break;
      }

      case ',': {
        m_wantIndent = true;
        buffer.writeCharSimple(',');
        break;
      }

      case '"': {
        if(m_wantIndent) {
          writeIndent(&buffer);
        }
        buffer.writeCharSimple('"');
        m_wantIndent = false;
        m_isInString = !m_isInString;
        break;
      }

      case ':': {
        if(!m_isInString) {
          buffer.writeSimple(": ", 2);
        } else {
          buffer.writeCharSimple(':');
        }
        break;
      }

      default:
        buffer.writeCharSimple(c);

    }

  }

  return m_outputStream->writeSimple(buffer.getData(), buffer.getCurrentPosition());

}


void Beautifier::setOutputStreamIOMode(IOMode ioMode) {
  return m_outputStream->setOutputStreamIOMode(ioMode);
}


Beautifier::IOMode Beautifier::getOutputStreamIOMode() {
  return m_outputStream->getOutputStreamIOMode();
}

Beautifier::Context& Beautifier::getOutputStreamContext() {
  return m_outputStream->getOutputStreamContext();
}

}}}
