// Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef __LOGGER_MANAGER_IMPL_H
#define __LOGGER_MANAGER_IMPL_H

#include <log/logger_specification.h>

// Forward declaration to avoid need to include log4cplus header file here.
namespace log4cplus {
class Logger;
}

namespace isc {
namespace log {

/// \brief Logger Manager Implementation
///
/// This is the implementation of the logger manager for the log4cplus
/// underlying logger.
///
/// As noted in logger_manager.h, the logger manager class exists to set up the
/// logging given a set of specifications.  This class handles the processing
/// of those specifications.

class LoggerManagerImpl {
public:

    /// \brief Constructor
    LoggerManagerImpl()
    {}

    /// \brief Initialize Processing
    ///
    /// This resets the hierachy of loggers back to their defaults.  This means
    /// that all non-root loggers (if they exist) are set to NOT_SET, and the
    /// root logger reset to logging informational messages.
    void processInit();

    /// \brief Process Specification
    ///
    /// Processes the specification for a single logger.
    ///
    /// \param spec Logging specification for this logger
    void processSpecification(const LoggerSpecification& spec);

    /// \brief End Processing
    ///
    /// Terminates the processing of the logging specifications.
    void processEnd();

private:
    /// \brief Create console appender
    ///
    /// Creates an object that, when attached to a logger, will log to one
    /// of the output streams (stdout or stderr).
    ///
    /// \param logger Log4cplus logger to which the appender must be attached.
    /// \param opt Output options for this appender.
    void createConsoleAppender(log4cplus::Logger& logger,
                               const OutputOption& opt);

    /// \brief Create file appender
    ///
    /// Creates an object that, when attached to a logger, will log to a
    /// specified file.  This also includes the ability to "roll" files when
    /// they reach a specified size.
    ///
    /// \param logger Log4cplus logger to which the appender must be attached.
    /// \param opt Output options for this appender.
    void createFileAppender(log4cplus::Logger& logger,
                            const OutputOption& opt) {}

    /// \brief Create syslog appender
    ///
    /// Creates an object that, when attached to a logger, will log to the
    /// syslog file.
    ///
    /// \param logger Log4cplus logger to which the appender must be attached.
    /// \param opt Output options for this appender.
    void createSyslogAppender(log4cplus::Logger& logger,
                              const OutputOption& opt) {}
};

} // namespace log
} // namespace isc

#endif // __LOGGER_MANAGER_IMPL_H
