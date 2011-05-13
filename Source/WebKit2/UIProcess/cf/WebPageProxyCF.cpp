/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WebPageProxy.h"

#include "DataReference.h"
#include "Logging.h"
#include "SessionState.h"
#include "WebBackForwardList.h"
#include "WebData.h"
#include "WebPageMessages.h"
#include "WebProcessProxy.h"

#include <wtf/RetainPtr.h>
#include <CoreFoundation/CFPropertyList.h>

using namespace WebCore;

namespace WebKit {

DEFINE_STATIC_GETTER(CFStringRef, SessionHistoryKey, (CFSTR("SessionHistory")));

static const UInt32 CurrentSessionStateDataVersion = 2;

PassRefPtr<WebData> WebPageProxy::sessionStateData(WebPageProxySessionStateFilterCallback filter, void* context) const
{
    RetainPtr<CFDictionaryRef> sessionHistoryDictionary(AdoptCF, m_backForwardList->createCFDictionaryRepresentation(filter, context));
    
    // For now we're only serializing the back/forward list.  If that object is null, then the entire sessionState can be null.
    if (!sessionHistoryDictionary)
        return 0;
    
    const void* keys[1] = { SessionHistoryKey() };
    const void* values[1] = { sessionHistoryDictionary.get() };
    
    RetainPtr<CFDictionaryRef> stateDictionary(AdoptCF, CFDictionaryCreate(0, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    RetainPtr<CFWriteStreamRef> writeStream(AdoptCF, CFWriteStreamCreateWithAllocatedBuffers(0, 0));
    if (!writeStream)
        return 0;
    
    if (!CFWriteStreamOpen(writeStream.get()))
        return 0;
        
    if (!CFPropertyListWriteToStream(stateDictionary.get(), writeStream.get(), kCFPropertyListBinaryFormat_v1_0, 0))
        return 0;
        
    RetainPtr<CFDataRef> stateCFData(AdoptCF, (CFDataRef)CFWriteStreamCopyProperty(writeStream.get(), kCFStreamPropertyDataWritten));

    CFIndex length = CFDataGetLength(stateCFData.get());
    Vector<unsigned char> stateVector(length + sizeof(UInt32));
    
    // Put the session state version number at the start of the buffer
    stateVector.data()[0] = (CurrentSessionStateDataVersion & 0xFF000000) >> 24;
    stateVector.data()[1] = (CurrentSessionStateDataVersion & 0x00FF0000) >> 16;
    stateVector.data()[2] = (CurrentSessionStateDataVersion & 0x0000FF00) >> 8;
    stateVector.data()[3] = (CurrentSessionStateDataVersion & 0x000000FF);
    
    // Copy in the actual session state data
    CFDataGetBytes(stateCFData.get(), CFRangeMake(0, length), stateVector.data() + sizeof(UInt32));
    
    return WebData::create(stateVector);
}

void WebPageProxy::restoreFromSessionStateData(WebData* webData)
{
    if (!webData || webData->size() < sizeof(UInt32))
        return;

    const unsigned char* buffer = webData->bytes();
    UInt32 versionHeader = (buffer[0] << 24) + (buffer[1] << 16) + (buffer[2] << 8) + buffer[3];
    
    if (versionHeader != CurrentSessionStateDataVersion) {
        LOG(SessionState, "Unrecognized version header for session state data - cannot restore");
        return;
    }
    
    RetainPtr<CFDataRef> data(AdoptCF, CFDataCreate(0, webData->bytes() + sizeof(UInt32), webData->size() - sizeof(UInt32)));

    CFStringRef propertyListError = 0;
    RetainPtr<CFPropertyListRef> propertyList(AdoptCF, CFPropertyListCreateFromXMLData(0, data.get(), kCFPropertyListImmutable, &propertyListError));
    if (propertyListError) {
        CFRelease(propertyListError);
        LOG(SessionState, "Could not read session state property list");
        return;
    }

    if (!propertyList)
        return;
        
    if (CFGetTypeID(propertyList.get()) != CFDictionaryGetTypeID()) {
        LOG(SessionState, "SessionState property list is not a CFDictionaryRef (%i) - its CFTypeID is %i", (int)CFDictionaryGetTypeID(), (int)CFGetTypeID(propertyList.get()));
        return;
    }
    
    CFTypeRef sessionHistoryRef = CFDictionaryGetValue(static_cast<CFDictionaryRef>(propertyList.get()), SessionHistoryKey());
    if (!sessionHistoryRef || CFGetTypeID(sessionHistoryRef) != CFDictionaryGetTypeID()) {
        LOG(SessionState, "SessionState dictionary does not contain a SessionHistoryDictionary key, it is of the wrong type");
        return;
    }
    
    CFDictionaryRef sessionHistoryDictionary = static_cast<CFDictionaryRef>(sessionHistoryRef);
    if (!m_backForwardList->restoreFromCFDictionaryRepresentation(sessionHistoryDictionary)) {
        LOG(SessionState, "Failed to restore back/forward list from SessionHistoryDictionary");
        return;
    }
    
    const BackForwardListItemVector& entries = m_backForwardList->entries();
    size_t size = entries.size();
    for (size_t i = 0; i < size; ++i)
        process()->registerNewWebBackForwardListItem(entries[i].get());

    process()->send(Messages::WebPage::RestoreSessionAndNavigateToCurrentItem(SessionState(m_backForwardList->entries(), m_backForwardList->currentIndex())), m_pageID);
}

} // namespace WebKit
