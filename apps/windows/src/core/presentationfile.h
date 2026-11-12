#pragma once

#include "core/document.h"

#include <QString>

namespace mxu {

// Reads and writes the same .mxuslides bundle the macOS app produces:
//   manifest.json   format marker and summary
//   presentation.json  { presentation, themes, media, audio }
//   blobs/<sha256>.<ext>  referenced media bytes
//   fonts/...       optional shared fonts (preserved on import, not embedded)
class PresentationFile {
public:
    static constexpr int FormatVersion = 1;

    static QString fileNameFor(const Presentation &presentation);
    static bool write(const Bundle &bundle, const QString &path, QString *error = nullptr);
    static bool read(const QString &path, Bundle *bundle, QString *error = nullptr);
};

} // namespace mxu
