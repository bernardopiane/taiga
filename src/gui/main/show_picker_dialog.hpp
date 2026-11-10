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

#include <QDialog>
#include <optional>
#include <vector>

#include "media/anime.hpp"

class QLineEdit;
class QListWidget;
class QSpinBox;

namespace gui {

class ShowPickerDialog final : public QDialog {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(ShowPickerDialog)

public:
  struct Selection {
    int animeId = 0;
    int episodeNumber = 1;
  };

  ShowPickerDialog(QWidget* parent, const std::vector<Anime>& items);
  ~ShowPickerDialog() = default;

  static std::optional<Selection> choose(QWidget* parent, const std::vector<Anime>& items);

private:
  struct Entry {
    int id = 0;
    QString title;
    QString keywords;
  };

  void filterItems(const QString& text);
  void updateEpisodeNumber();

  QLineEdit* m_filterBox = nullptr;
  QListWidget* m_listWidget = nullptr;
  QSpinBox* m_episodeSpin = nullptr;
  std::vector<Entry> m_entries;
};

}  // namespace gui
