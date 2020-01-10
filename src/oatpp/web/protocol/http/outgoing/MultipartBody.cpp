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

#include "MultipartBody.hpp"

#include "oatpp/core/data/stream/ChunkedBuffer.hpp"

namespace oatpp { namespace web { namespace protocol { namespace http { namespace outgoing {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MultipartReadCallback

MultipartBody::MultipartReadCallback::MultipartReadCallback(const std::shared_ptr<Multipart>& multipart)
  : m_multipart(multipart)
  , m_iterator(multipart->getAllParts().begin())
  , m_state(STATE_BOUNDARY)
  , m_readStream(nullptr, nullptr, 0)
{}

v_io_size MultipartBody::MultipartReadCallback::readBody(void *buffer, v_buff_size count, async::Action& action) {
  auto& part = *m_iterator;
  const auto& stream = part->getInputStream();
  if(!stream) {
    OATPP_LOGW("[oatpp::web::protocol::http::outgoing::MultipartBody::MultipartReadCallback::readBody()]", "Warning. Part has no input stream", m_state);
    m_iterator ++;
    return 0;
  }
  auto res = stream->read(buffer, count, action);
  if(res == 0) {
    m_iterator ++;
  }
  return res;
}

v_io_size MultipartBody::MultipartReadCallback::read(void *buffer, v_buff_size count, async::Action& action) {

  if(m_state == STATE_FINISHED) {
    return 0;
  }

  p_char8 currBufferPtr = (p_char8) buffer;
  v_io_size bytesLeft = count;

  v_io_size res = 0;

  while(bytesLeft > 0 && action.isNone()) {

    switch (m_state) {

      case STATE_BOUNDARY:
        res = readBoundary(m_multipart, m_iterator, m_readStream, currBufferPtr, bytesLeft);
        break;

      case STATE_HEADERS:
        res = readHeaders(m_multipart, m_iterator, m_readStream, currBufferPtr, bytesLeft);
        break;

      case STATE_BODY:
        res = readBody(currBufferPtr, bytesLeft, action);
        break;

      default:
        OATPP_LOGE("[oatpp::web::protocol::http::outgoing::MultipartBody::MultipartReadCallback::read()]", "Error. Invalid state %d", m_state);
        return 0;

    }

    if(res > 0) {
      currBufferPtr = &currBufferPtr[res];
      bytesLeft -= res;
    } else if(res == 0) {

      if(m_state == STATE_BOUNDARY && m_iterator == m_multipart->getAllParts().end()) {
        m_state = STATE_FINISHED;
        break;
      }

      m_state += 1;
      if(m_state == STATE_ROUND) {
        m_state = 0;
      }

    } else if(action.isNone()) {
      OATPP_LOGE("[oatpp::web::protocol::http::outgoing::MultipartBody::MultipartReadCallback::read()]", "Error. Invalid read result %d. State=%d", res, m_state);
      return 0;
    }

  }

  return count - bytesLeft;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MultipartBody

v_io_size MultipartBody::readBoundary(const std::shared_ptr<Multipart>& multipart,
                                            std::list<std::shared_ptr<Part>>::const_iterator& iterator,
                                            data::stream::BufferInputStream& readStream,
                                            void *buffer,
                                            v_buff_size count)
{
  if (!readStream.getDataMemoryHandle()) {

    oatpp::String boundary;

    if (iterator == multipart->getAllParts().end()) {
      boundary = "\r\n--" + multipart->getBoundary() + "--\r\n";
    } else if (iterator == multipart->getAllParts().begin()) {
      boundary = "--" + multipart->getBoundary() + "\r\n";
    } else {
      boundary = "\r\n--" + multipart->getBoundary() + "\r\n";
    }

    readStream.reset(boundary.getPtr(), boundary->getData(), boundary->getSize());

  }

  auto res = readStream.readSimple(buffer, count);
  if(res == 0) {
    readStream.reset();
  }

  return res;
}

v_io_size MultipartBody::readHeaders(const std::shared_ptr<Multipart>& multipart,
                                           std::list<std::shared_ptr<Part>>::const_iterator& iterator,
                                           data::stream::BufferInputStream& readStream,
                                           void *buffer,
                                           v_buff_size count)
{
  (void) multipart;

  if (!readStream.getDataMemoryHandle()) {

    oatpp::data::stream::ChunkedBuffer stream;
    auto& part = *iterator;
    http::Utils::writeHeaders(part->getHeaders(), &stream);
    stream.writeSimple("\r\n", 2);
    auto str = stream.toString();
    readStream.reset(str.getPtr(), str->getData(), str->getSize());

  }

  auto res = readStream.readSimple(buffer, count);
  if(res == 0) {
    readStream.reset();
  }

  return res;
}

MultipartBody::MultipartBody(const std::shared_ptr<Multipart>& multipart)
  : ChunkedBody(std::make_shared<MultipartReadCallback>(multipart))
  , m_multipart(multipart)
{}

void MultipartBody::declareHeaders(Headers& headers) {
  if(m_multipart->getAllParts().empty()) {
    headers.put_LockFree(oatpp::web::protocol::http::Header::CONTENT_LENGTH, "0");
    return;
  }
  ChunkedBody::declareHeaders(headers);
  headers.put_LockFree(oatpp::web::protocol::http::Header::CONTENT_TYPE, "multipart/form-data; boundary=" + m_multipart->getBoundary());
}

void MultipartBody::writeToStream(OutputStream* stream) {
  if(m_multipart->getAllParts().empty()) {
    return;
  }
  ChunkedBody::writeToStream(stream);
}

oatpp::async::CoroutineStarter MultipartBody::writeToStreamAsync(const std::shared_ptr<OutputStream>& stream) {
  if(m_multipart->getAllParts().empty()) {
    return nullptr;
  }
  return ChunkedBody::writeToStreamAsync(stream);
}

v_buff_size MultipartBody::getKnownSize() {
 return -1;
}

}}}}}
