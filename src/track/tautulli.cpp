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

#include "tautulli.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QRestAccessManager>
#include <QRestReply>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>
#include <chrono>

#include "base/log.hpp"
#include "base/string.hpp"
#include "taiga/config.h"
#include "taiga/network.hpp"
#include "taiga/settings.hpp"
#include "track/media.hpp"
#include "track/recognition.hpp"

namespace track::tautulli {

TautulliClient::TautulliClient(QObject* parent) : QObject(parent), manager_{taiga::network(), this} {
  pollTimer_ = new QTimer(this);
  connect(pollTimer_, &QTimer::timeout, this, &TautulliClient::poll);
}

bool TautulliClient::init() {
  setPollingEnabled(isEnabled());
  return true;
}

bool TautulliClient::isEnabled() const {
  return taiga::settings.tautulliEnabled();
}

void TautulliClient::setEnabled(bool enabled) {
  taiga::settings.setTautulliEnabled(enabled);
  setPollingEnabled(enabled);
}

bool TautulliClient::isPlaying() const {
  return playing_;
}

const std::optional<Episode> TautulliClient::getCurrentEpisode() const {
  return currentEpisode_;
}

////////////////////////////////////////////////////////////////////////////////
// Polling

void TautulliClient::setPollingEnabled(bool enabled) {
  if (enabled) {
    pollTimer_->start(taiga::settings.mediaDetectionInterval());
  } else {
    pollTimer_->stop();
    reset();
  }
}

QUrl getActivityUrl(const QString& host, const QString& apiKey) {
  QString schemelessHost = host;

  if (!schemelessHost.contains(u"://"_s)) {
    schemelessHost.prepend(u"http://"_s);
  }

  QUrl url{schemelessHost};

  if (!url.isValid() || url.host().isEmpty()) {
    return {};
  }

  // Remove trailing slashes, then append the API path.
  QString path = url.path();
  while (path.endsWith(u'/')) path.chop(1);
  url.setPath(path + u"/api/v2"_s);

  QUrlQuery query;
  query.addQueryItem(u"cmd"_s, u"get_activity"_s);
  query.addQueryItem(u"apikey"_s, apiKey);
  url.setQuery(query);

  return url;
}

std::optional<Episode> parseSession(const QJsonObject& session) {
  // Only episodes are supported, since movies cannot be identified in the
  // same way.
  const auto mediaType = session.value("media_type").toString();
  if (mediaType.isEmpty() || mediaType != u"episode"_s) {
    return std::nullopt;
  }

  const auto showTitle = session.value("grandparent_title").toString();
  const auto seasonNumber = session.value("parent_media_index").toInt();
  const auto episodeNumber = session.value("media_index").toInt();

  if (showTitle.isEmpty() || seasonNumber < 1 || episodeNumber < 1) {
    return std::nullopt;
  }

  Episode episode;
  episode.addElement(anitomy::ElementKind::Title, showTitle.toStdString());
  episode.addElement(anitomy::ElementKind::Season, std::to_string(seasonNumber));
  episode.addElement(anitomy::ElementKind::Episode, std::to_string(episodeNumber));

  if (const auto episodeTitle = session.value("title").toString(); !episodeTitle.isEmpty()) {
    episode.addElement(anitomy::ElementKind::EpisodeTitle, episodeTitle.toStdString());
  }

  const auto animeId = track::recognition::identify(episode);
  episode.setAnimeId(animeId);

  return episode;
}

std::optional<Episode> findEpisode(const QJsonArray& sessions, bool& playing) {
  // Prefer the first session that is currently playing, and otherwise fall
  // back to the first tracked episode (e.g. paused while the user stepped
  // out). Sessions of other people in the household aren't excluded here,
  // since a local library is typically being watched from this device.
  for (const bool onlyPlaying : {true, false}) {
    for (const auto& value : sessions) {
      const auto session = value.toObject();

      const auto isPlaying = [&] {
        const auto state = session.value("state").toString();
        // Treat buffering like playing, because querying happens often enough
        // that buffering doesn't meaningfully change the estimated progress.
        return state == u"playing"_s || state == u"buffering"_s;
      }();

      auto episode = parseSession(session);
      if (!episode) continue;
      if (onlyPlaying && !isPlaying) continue;

      playing = isPlaying;
      return episode;
    }
  }

  return std::nullopt;
}

void TautulliClient::poll() {
  const QString host = QString::fromStdString(taiga::settings.tautulliHost());
  const QString apiKey = QString::fromStdString(taiga::settings.tautulliApiKey());

  if (isSuppressedByLocalPlayback()) {
    // Local playback took over. Forget about the Plex session without
    // signalling a media close, so the local playback session isn't broken.
    currentEpisode_.reset();
    return;
  }

  if (host.isEmpty() || apiKey.isEmpty()) {
    // Tautulli isn't configured yet.
    reset();
    return;
  }

  const auto url = getActivityUrl(host, apiKey);
  if (!url.isValid() || url.isEmpty()) {
    reset();
    return;
  }

  QNetworkRequest request{url};
  request.setTransferTimeout(std::chrono::milliseconds{5000});
  request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader,
                    QString::fromLatin1(TAIGA_APP_NAME));
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::RedirectPolicy::NoLessSafeRedirectPolicy);

  manager_.get(request, this, [this](QRestReply& reply) {
    const auto json = reply.readJson();

    if (!json || !json->isObject()) {
      // Connection problems are kept silent, since the next poll is soon to
      // follow and the user may not want to be notified while the media
      // player is playing.
      return;
    }

    const auto data = json->object().value("data").toObject();
    const auto sessions = data.value("sessions").toArray();

    if (data.isEmpty() || sessions.isEmpty()) {
      reset();
      return;
    }

    bool playing = true;
    auto episode = findEpisode(sessions, playing);

    if (!episode) {
      reset();
      return;
    }

    playing_ = playing;

    if (hasEpisodeChanged(*episode)) {
      currentEpisode_ = episode;
      emit currentEpisodeChanged(episode);
    }
  });
}

bool TautulliClient::isSuppressedByLocalPlayback() const {
  return track::media::detection()->getCurrentEpisode().has_value();
}

void TautulliClient::reset() {
  playing_ = false;

  if (currentEpisode_) {
    emit currentEpisodeChanged(std::nullopt);
  }
  currentEpisode_.reset();
}

bool TautulliClient::hasEpisodeChanged(const Episode& episode) const {
  if (!currentEpisode_) return true;
  if (currentEpisode_->animeId() != episode.animeId()) return true;

  return currentEpisode_->elements(anitomy::ElementKind::Episode) ==
         episode.elements(anitomy::ElementKind::Episode);
}

////////////////////////////////////////////////////////////////////////////////
// Connection test

void TautulliClient::testConnection(
    QObject* context, const std::string& host, const std::string& apiKey,
    const std::function<void(bool success, const std::string& message)>& callback) {
  const auto url = getActivityUrl(QString::fromStdString(host), QString::fromStdString(apiKey));

  if (!url.isValid() || url.isEmpty()) {
    callback(false, "Invalid host. Example: http://localhost:8181");
    return;
  }

  QNetworkRequest request{url};
  request.setTransferTimeout(std::chrono::milliseconds{5000});
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::RedirectPolicy::NoLessSafeRedirectPolicy);

  manager_.get(request, context, [callback](QRestReply& reply) {
    const auto json = reply.readJson();

    if (!json || !json->isObject()) {
      callback(false, "Could not connect to Tautulli. Check the host address.");
      return;
    }

    const auto result = json->object().value("result").toString();

    if (result != "success") {
      // Tautulli reports an invalid API key as an "error" result.
      callback(false, "Could not connect to Tautulli. Check the API key.");
      return;
    }

    const auto sessions =
        json->object().value("data").toObject().value("sessions").toArray().size();

    callback(true,
             u"Connected to Tautulli. %1 active session(s)."_s.arg(sessions).toStdString());
  });
}

}  // namespace track::tautulli
