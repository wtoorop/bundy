// Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

// $Id$

#ifndef __RRCLASS_H
#define __RRCLASS_H 1

#include "exceptions.h"

namespace isc {
namespace dns {

// forward declarations
class OutputBuffer;
class MessageRenderer;

///
/// \brief TBD
///
class InvalidRRClass : public Exception {
public:
    InvalidRRClass(const char* file, size_t line, const char* what) :
        isc::dns::Exception(file, line, what) {}
};

class RRClass {
public:
    ///
    /// \name Constructors and Destructor
    ///
    //@{
    explicit RRClass(uint16_t classcode) : classcode_(classcode) {}
    explicit RRClass(const std::string& classstr);
    ///
    /// We use the default copy constructor intentionally.
    //@}
    /// We use the default copy assignment operator intentionally.
    ///
    const std::string toText() const;
    void toWire(OutputBuffer& buffer) const;
    void toWire(MessageRenderer& renderer) const;
    uint16_t getCode() const { return (classcode_); }
    bool operator==(const RRClass& other) const
    { return (classcode_ == other.classcode_); }
    bool operator!=(const RRClass& other) const
    { return (classcode_ != other.classcode_); }
    ///
    /// We define this so that RRClasss can be stored in STL containers.
    ///
    bool operator<(const RRClass& other) const
    { return (classcode_ < other.classcode_); }

    // (Some) Well-known RRclass constants
    static const RRClass& IN();

private:
    uint16_t classcode_;
};

// We'll probably auto generate this code.  But in this preliminary version
// we simply hard code some selected classes.
inline const RRClass&
RRClass::IN()
{
    static RRClass rrclass(1);

    return (rrclass);
}

std::ostream&
operator<<(std::ostream& os, const RRClass& rrclass);
}
}
#endif  // __RRCLASS_H

// Local Variables: 
// mode: c++
// End: 
