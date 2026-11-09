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

#include <QLabel>
#include <QObject>
#include <QPixmap>
#include <QWidget>
#include <optional>

#include "gui/common/clickable_label.hpp"
#include "gui/common/poster_widget.hpp"
#include "media/anime.hpp"
#include "track/episode.hpp"

class QFormLayout;

namespace gui {

class HomeWidget final : public QWidget {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(HomeWidget)

public:
  HomeWidget(QWidget* parent);
  ~HomeWidget() = default;

protected:
  void changeEvent(QEvent* event) override;

private:
  QWidget* makeSection(const QString& title, QWidget* content);
  ClickableLabel* makeAction(const QString& text);

  std::optional<track::Episode> currentEpisode() const;

  void refresh();
  void render();

  void applyPaletteColors();
  void showStatusMessage(const QString& text) const;

  // Header
  PosterWidget* m_posterWidget = nullptr;
  ClickableLabel* m_titleLabel = nullptr;
  QLabel* m_nowPlayingLabel = nullptr;
  ClickableLabel* m_editAction = nullptr;
  ClickableLabel* m_shareAction = nullptr;
  ClickableLabel* m_nextEpisodeAction = nullptr;

  // Alternative titles
  QWidget* m_altTitlesSection = nullptr;
  QLabel* m_altTitlesLabel = nullptr;

  // Details
  QWidget* m_detailsSection = nullptr;
  QFormLayout* m_detailsLayout = nullptr;

  // Synopsis
  QWidget* m_synopsisSection = nullptr;
  QLabel* m_synopsisLabel = nullptr;

  // States
  QWidget* m_content = nullptr;
  QLabel* m_placeholderLabel = nullptr;

  std::optional<Anime> m_anime;
  std::optional<track::Episode> m_episode;
};

}  // namespace gui
