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
#include "show_picker_dialog.hpp"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>

#include "base/string.hpp"
#include "media/anime_utils.hpp"
#include "track/play.hpp"

namespace gui {

namespace {

constexpr int kMaxResults = 200;

}  // namespace

ShowPickerDialog::ShowPickerDialog(QWidget* parent, const std::vector<Anime>& items)
    : QDialog(parent) {
  setWindowTitle(tr("Set current show"));
  setMinimumSize(420, 480);

  const auto rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(12, 12, 12, 12);
  rootLayout->setSpacing(8);

  m_filterBox = new QLineEdit(this);
  m_filterBox->setPlaceholderText(tr("Search shows..."));
  m_filterBox->setClearButtonEnabled(true);
  rootLayout->addWidget(m_filterBox);

  m_listWidget = new QListWidget(this);
  m_listWidget->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
  rootLayout->addWidget(m_listWidget, 1);

  const auto episodeLayout = new QHBoxLayout();
  episodeLayout->setSpacing(8);
  auto* episodeLabel = new QLabel(tr("Episode:"), this);
  episodeLayout->addWidget(episodeLabel);

  m_episodeSpin = new QSpinBox(this);
  m_episodeSpin->setRange(1, anime::kMaxEpisodeCount);
  m_episodeSpin->setValue(1);
  episodeLayout->addWidget(m_episodeSpin, 1);
  rootLayout->addLayout(episodeLayout);

  const auto buttonBox = new QDialogButtonBox(
      QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel, this);
  rootLayout->addWidget(buttonBox);

  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_filterBox, &QLineEdit::textChanged, this, &ShowPickerDialog::filterItems);
  connect(m_listWidget, &QListWidget::itemSelectionChanged, this,
          &ShowPickerDialog::updateEpisodeNumber);

  m_entries.reserve(items.size());
  for (const auto& item : items) {
    auto title = QString::fromStdString(anime::preferredTitle(item));
    auto keywords = title;
    for (const auto& synonym : item.titles.synonyms) {
      keywords += u" "_s + QString::fromStdString(synonym);
    }
    keywords += u" "_s + QString::fromStdString(item.titles.romaji);
    keywords += u" "_s + QString::fromStdString(item.titles.english);
    keywords += u" "_s + QString::fromStdString(item.titles.japanese);
    m_entries.push_back({item.id, title, keywords});
  }

  std::sort(m_entries.begin(), m_entries.end(), [](const auto& a, const auto& b) {
    return a.title.compare(b.title, Qt::CaseSensitivity::CaseInsensitive) < 0;
  });

  filterItems(QString{});
}

std::optional<ShowPickerDialog::Selection> ShowPickerDialog::choose(
    QWidget* parent, const std::vector<Anime>& items) {
  auto dialog = ShowPickerDialog(parent, items);

  if (dialog.exec() != QDialog::DialogCode::Accepted) return std::nullopt;

  const auto* item = dialog.m_listWidget->currentItem();
  if (!item) return std::nullopt;

  return Selection{item->data(Qt::ItemDataRole::UserRole).toInt(),
                   dialog.m_episodeSpin->value()};
}

void ShowPickerDialog::filterItems(const QString& text) {
  m_listWidget->clear();

  const auto query = text.trimmed();
  int count = 0;

  for (const auto& entry : m_entries) {
    if (count >= kMaxResults) break;
    if (!query.isEmpty() && !entry.keywords.contains(query, Qt::CaseSensitivity::CaseInsensitive)) {
      continue;
    }

    auto* item = new QListWidgetItem(entry.title, m_listWidget);
    item->setData(Qt::ItemDataRole::UserRole, entry.id);
    count++;
  }
}

void ShowPickerDialog::updateEpisodeNumber() {
  const auto* item = m_listWidget->currentItem();
  if (!item) return;

  const auto id = item->data(Qt::ItemDataRole::UserRole).toInt();
  const auto number = track::nextEpisodeNumber(id);
  if (number) m_episodeSpin->setValue(*number);
}

}  // namespace gui
