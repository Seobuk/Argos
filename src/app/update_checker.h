/****************************************************************************
** Copyright (c) 2016, Fougue SAS <https://www.fougue.pro>
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#pragma once

class QString;
class QWidget;

namespace Mayo {

// Queries GitHub for the latest Argos release and, if newer than the running build,
// offers to open the download page. When silentIfUpToDate is true, no dialog is shown
// unless a newer version is found (used for the one-shot startup check).
void checkForUpdates(QWidget* parent, bool silentIfUpToDate);

// Pure version comparison: returns true if remoteTag (eg "v1.2.0") is newer than
// localVersion (eg "1.1.9"). Compares the first three numeric components; missing => 0.
bool isRemoteNewer(const QString& remoteTag, const QString& localVersion);

} // namespace Mayo
