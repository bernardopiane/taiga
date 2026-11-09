/**
 * Taiga
 * Copyright (C) 2010-2026, Eren Okka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QApplication>
#include <QObject>
#include <QRestAccessManager>
#include <QTimer>
#include <functional>
#include <optional>
#include <string>

#include "track/episode.hpp"

namespace track::tautulli {

class TautulliClient final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(TautulliClient)

public:
  TautulliClient(QObject* parent);

  bool init();

  bool isEnabled() const;
  void setEnabled(bool enabled);

  const std::optional<Episode> getCurrentEpisode() const;

  // Whether the active Plex session is currently playing (not paused).
  bool isPlaying() const;

  void testConnection(QObject* context, const std::string& host, const std::string& apiKey,
                      const std::function<void(bool success, const std::string& message)>& callback);

signals:
  void currentEpisodeChanged(std::optional<Episode> media) const;

private:
  void setPollingEnabled(bool enabled);
  void poll();
  void reset();

  bool hasEpisodeChanged(const Episode& episode) const;

  // Local playback takes precedence over Plex streaming, so a Plex session is
  // only tracked when nothing is playing locally.
  bool isSuppressedByLocalPlayback() const;

  std::optional<Episode> currentEpisode_;
  bool playing_ = false;

  QTimer* pollTimer_;
  QRestAccessManager manager_;
};

inline TautulliClient* client() {
  static auto client = new TautulliClient(qApp);
  return client;
}

}  // namespace track::tautulli
