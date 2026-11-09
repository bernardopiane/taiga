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
#include "home_widget.hpp"

#include <QClipboard>
#include <QDesktopServices>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "base/string.hpp"
#include "gui/common/clickable_label.hpp"
#include "gui/common/poster_widget.hpp"
#include "gui/main/main_window.hpp"
#include "gui/main/status_bar_controller.hpp"
#include "gui/media/media_dialog.hpp"
#include "gui/utils/format.hpp"
#include "gui/utils/image_provider.hpp"
#include "media/anime_db.hpp"
#include "media/anime_season.hpp"
#include "media/anime_utils.hpp"
#include "sync/service.hpp"
#include "track/media.hpp"
#include "track/play.hpp"
#include "track/tautulli.hpp"

namespace gui {

namespace {

constexpr int kPosterWidth = 150;
constexpr int kPosterHeight = 225;
constexpr int kHorizontalMargin = 16;
constexpr int kVerticalMargin = 12;
constexpr int kSpacing = 12;

}  // namespace

HomeWidget::HomeWidget(QWidget* parent) : QWidget(parent) {
  const auto rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  const auto scrollArea = new QScrollArea(this);
  scrollArea->setFrameShape(QFrame::Shape::NoFrame);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setWidgetResizable(true);
  rootLayout->addWidget(scrollArea);

  m_content = new QWidget(this);
  scrollArea->setWidget(m_content);

  const auto contentLayout = new QVBoxLayout(m_content);
  contentLayout->setContentsMargins(kHorizontalMargin, kVerticalMargin, kHorizontalMargin,
                                    kVerticalMargin);
  contentLayout->setSpacing(kSpacing);

  // Header
  {
    const auto headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(kSpacing);
    contentLayout->addLayout(headerLayout);

    m_posterWidget = new PosterWidget(this);
    m_posterWidget->setFixedSize(kPosterWidth, kPosterHeight);
    headerLayout->addWidget(m_posterWidget, 0, Qt::AlignTop);
    connect(m_posterWidget, &PosterWidget::clicked, this, [this](Qt::MouseButton button) {
      if (button == Qt::MouseButton::LeftButton && m_anime) {
        QDesktopServices::openUrl(QUrl{sync::animePageUrl(m_anime->id)});
      }
    });

    const auto textLayout = new QVBoxLayout();
    textLayout->setSpacing(4);
    headerLayout->addLayout(textLayout, 1);

    m_titleLabel = new ClickableLabel(this);
    m_titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    {
      auto titleFont = m_titleLabel->font();
      titleFont.setPointSize(titleFont.pointSize() + 4);
      titleFont.setWeight(QFont::DemiBold);
      m_titleLabel->setFont(titleFont);
    }
    textLayout->addWidget(m_titleLabel);
    connect(m_titleLabel, &ClickableLabel::clicked, this, [this](Qt::MouseButton button) {
      if (button == Qt::MouseButton::LeftButton && m_anime) {
        QDesktopServices::openUrl(QUrl{sync::animePageUrl(m_anime->id)});
      }
    });

    m_nowPlayingLabel = new QLabel(this);
    m_nowPlayingLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    textLayout->addWidget(m_nowPlayingLabel);

    const auto actionsLayout = new QHBoxLayout();
    actionsLayout->setSpacing(12);
    textLayout->addLayout(actionsLayout);
    textLayout->addStretch();

    m_editAction = makeAction(tr("Edit"));
    connect(m_editAction, &ClickableLabel::clicked, this, [this](Qt::MouseButton button) {
      if (button == Qt::MouseButton::LeftButton && m_anime) {
        MediaDialog::show(this, MediaDialogPage::Details, *m_anime);
      }
    });
    actionsLayout->addWidget(m_editAction);

    m_shareAction = makeAction(tr("Share"));
    connect(m_shareAction, &ClickableLabel::clicked, this, [this](Qt::MouseButton button) {
      if (button == Qt::MouseButton::LeftButton && m_anime) {
        QGuiApplication::clipboard()->setText(sync::animePageUrl(m_anime->id));
        showStatusMessage(tr("Link copied to clipboard"));
      }
    });
    actionsLayout->addWidget(m_shareAction);

    m_nextEpisodeAction = makeAction(tr("Watch next episode"));
    connect(m_nextEpisodeAction, &ClickableLabel::clicked, this,
            [this](Qt::MouseButton button) {
              if (button != Qt::MouseButton::LeftButton || !m_anime) return;
              const auto number = track::nextEpisodeNumber(m_anime->id);
              if (!number || track::playEpisode(m_anime->id, *number)) return;
              showStatusMessage(
                  tr("Episode %1 could not be found in library folders.").arg(*number));
            });
    actionsLayout->addWidget(m_nextEpisodeAction);

    textLayout->addStretch();
  }

  // Alternative titles
  {
    m_altTitlesLabel = new QLabel(this);
    m_altTitlesLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_altTitlesLabel->setWordWrap(true);
    m_altTitlesSection = makeSection(tr("Alternative titles"), m_altTitlesLabel);
    contentLayout->addWidget(m_altTitlesSection);
  }

  // Details
  {
    const auto detailsWidget = new QWidget(this);
    m_detailsLayout = new QFormLayout(detailsWidget);
    m_detailsLayout->setContentsMargins(0, 0, 0, 0);
    m_detailsSection = makeSection(tr("Details"), detailsWidget);
    contentLayout->addWidget(m_detailsSection);
  }

  // Synopsis
  {
    m_synopsisLabel = new QLabel(this);
    m_synopsisLabel->setTextFormat(Qt::TextFormat::RichText);
    m_synopsisLabel->setTextInteractionFlags(Qt::TextInteractionFlag::TextSelectableByMouse);
    m_synopsisLabel->setWordWrap(true);
    m_synopsisLabel->setContextMenuPolicy(Qt::ContextMenuPolicy::NoContextMenu);
    m_synopsisSection = makeSection(tr("Synopsis"), m_synopsisLabel);
    contentLayout->addWidget(m_synopsisSection);
  }

  contentLayout->addStretch();

  // Placeholder, shown while nothing is being monitored.
  m_placeholderLabel = new QLabel(tr("Start playing an episode to see it here."), this);
  m_placeholderLabel->setAlignment(Qt::AlignCenter);
  m_placeholderLabel->setForegroundRole(QPalette::PlaceholderText);
  rootLayout->addWidget(m_placeholderLabel, 1);

  applyPaletteColors();
  refresh();

  connect(&anime::db, &anime::Database::itemUpdated, this, [this](const int id) {
    if (m_anime && m_anime->id == id) refresh();
  });
  connect(&imageProvider, &ImageProvider::posterChanged, this, [this](const int id) {
    if (m_anime && m_anime->id == id) refresh();
  });
  connect(track::media::detection(), &track::media::Detection::currentEpisodeChanged, this,
          [this](std::optional<track::Episode>) { refresh(); });
  connect(track::tautulli::client(), &track::tautulli::TautulliClient::currentEpisodeChanged,
          this, [this](std::optional<track::Episode>) { refresh(); });
}

QWidget* HomeWidget::makeSection(const QString& title, QWidget* content) {
  auto* section = new QWidget(this);
  auto* layout = new QVBoxLayout(section);
  layout->setContentsMargins(0, 12, 0, 0);
  layout->setSpacing(4);

  auto* label = new QLabel(title, this);
  {
    auto font = label->font();
    font.setWeight(QFont::DemiBold);
    label->setFont(font);
  }
  layout->addWidget(label);

  auto* line = new QFrame(this);
  line->setFrameShape(QFrame::Shape::HLine);
  line->setForegroundRole(QPalette::Mid);
  layout->addWidget(line);

  layout->addWidget(content);

  return section;
}

ClickableLabel* HomeWidget::makeAction(const QString& text) {
  auto* action = new ClickableLabel(this);
  action->setText(text);
  action->setCursor(Qt::PointingHandCursor);
  action->setEnabled(false);
  return action;
}

std::optional<track::Episode> HomeWidget::currentEpisode() const {
  // Local playback takes precedence over Plex streaming, as elsewhere.
  if (const auto episode = track::media::detection()->getCurrentEpisode()) {
    return *episode;
  }
  if (const auto& episode = track::tautulli::client()->getCurrentEpisode()) {
    return episode;
  }
  return std::nullopt;
}

void HomeWidget::changeEvent(QEvent* event) {
  if (event->type() == QEvent::PaletteChange) applyPaletteColors();
  QWidget::changeEvent(event);
}

void HomeWidget::refresh() {
  m_episode = currentEpisode();

  m_anime.reset();
  if (m_episode) {
    if (const auto* item = anime::db.item(m_episode->animeId())) {
      m_anime = *item;
    }
  }

  render();
}

void HomeWidget::render() {
  const bool idle = !m_anime && !m_episode;
  m_content->setVisible(!idle);
  m_placeholderLabel->setVisible(idle);
  if (idle) return;

  // Title
  QString title;
  if (m_anime) {
    title = QString::fromStdString(anime::preferredTitle(*m_anime));
  } else if (m_episode) {
    title = QString::fromStdString(m_episode->element(anitomy::ElementKind::Title));
  }
  m_titleLabel->setText(title);
  m_titleLabel->setCursor(m_anime ? Qt::PointingHandCursor : Qt::ArrowCursor);
  m_posterWidget->setCursor(m_anime ? Qt::PointingHandCursor : Qt::ArrowCursor);

  // Poster
  QPixmap poster;
  if (m_anime) poster = imageProvider.loadPoster(m_anime->id);
  m_posterWidget->setPixmap(poster);
  m_posterWidget->setLoading(m_anime && poster.isNull() && !m_anime->image_url.empty());
  if (m_anime && poster.isNull()) {
    imageProvider.fetchPoster(m_anime->id);
  }

  // Now playing line
  QString nowPlaying;
  if (m_episode) {
    nowPlaying = u"Now playing: Episode %1"_s.arg(
        formatEpisodeNumbers(m_episode->elements(anitomy::ElementKind::Episode)));
    if (m_episode->contains(anitomy::ElementKind::ReleaseGroup)) {
      nowPlaying += u" by %1"_s.arg(
          QString::fromStdString(m_episode->element(anitomy::ElementKind::ReleaseGroup)));
    }
  }
  m_nowPlayingLabel->setText(nowPlaying);

  // Actions
  for (auto* action : {m_editAction, m_shareAction, m_nextEpisodeAction}) {
    action->setEnabled(m_anime.has_value());
  }

  // Alternative titles
  QStringList altTitles;
  if (m_anime) {
    const auto addTitle = [&altTitles, &title](const std::string& t) {
      const QString translated = QString::fromStdString(t);
      if (!translated.isEmpty() && translated != title) altTitles.push_back(translated);
    };
    addTitle(m_anime->titles.romaji);
    addTitle(m_anime->titles.english);
    addTitle(m_anime->titles.japanese);
  }
  m_altTitlesLabel->setText(altTitles.join(u", "_s));
  m_altTitlesSection->setVisible(!altTitles.isEmpty());

  // Details
  while (m_detailsLayout->rowCount() > 0) {
    m_detailsLayout->removeRow(0);
  }
  if (m_anime) {
    const auto addRow = [this](const QString& title, const QString& value) {
      auto* valueLabel = new QLabel(value, this);
      valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
      m_detailsLayout->addRow(title, valueLabel);
    };
    addRow(u"Type: "_s, formatType(m_anime->type));
    addRow(u"Episodes: "_s, formatNumber(m_anime->episode_count, u"?"_s));
    addRow(u"Status: "_s, formatStatus(m_anime->status));
    addRow(u"Season: "_s, formatSeason(anime::Season(m_anime->date_started)));
    addRow(u"Genres: "_s, joinStrings(m_anime->genres, u"Unknown"_s));
    addRow(u"Producers: "_s, joinStrings(m_anime->producers, u"Unknown"_s));
    addRow(u"Score: "_s, formatScore(m_anime->score));
  }
  m_detailsSection->setVisible(m_anime.has_value());

  // Synopsis
  if (m_anime && !m_anime->synopsis.empty()) {
    m_synopsisLabel->setText(QString::fromStdString(m_anime->synopsis));
    m_synopsisSection->setVisible(true);
  } else {
    m_synopsisSection->setVisible(false);
  }

  // Fetch missing metadata, like the media dialog does.
  if (m_anime && anime::isStale(*m_anime)) {
    sync::fetchAnime(m_anime->id);
  }
}

void HomeWidget::applyPaletteColors() {
  const auto linkColor = palette().color(QPalette::ColorGroup::Normal, QPalette::ColorRole::Link);

  QPalette linkPalette;
  linkPalette.setColor(QPalette::WindowText, linkColor);
  m_titleLabel->setPalette(linkPalette);
  for (auto* action : {m_editAction, m_shareAction, m_nextEpisodeAction}) {
    action->setPalette(linkPalette);
    action->setForegroundRole(QPalette::WindowText);
  }
}

void HomeWidget::showStatusMessage(const QString& text) const {
  mainWindow()->statusBarController()->showMessage(
      {StatusBarController::Source::Selection, text, /*spin=*/false});
  QTimer::singleShot(3000, mainWindow()->statusBarController(),
                     [controller = mainWindow()->statusBarController()]() {
                       controller->clearMessage(StatusBarController::Source::Selection);
                     });
}

}  // namespace gui
