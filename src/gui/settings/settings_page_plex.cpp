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
#include "settings_page_plex.hpp"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include "gui/utils/theme.hpp"
#include "base/string.hpp"
#include "taiga/settings.hpp"
#include "track/tautulli.hpp"
#include "ui_settings_dialog.h"

namespace gui {

SettingsPagePlex::SettingsPagePlex(Ui::SettingsDialog* ui, QDialog* dialog)
    : SettingsPage(ui, dialog) {
  connect(ui_->tautulliEnabledCheckBox, &QCheckBox::toggled, ui_->tautulliGroupBox,
          &QWidget::setEnabled);

  connect(ui_->tautulliApiKeyEchoCheckBox, &QCheckBox::toggled, this, [this](const bool visible) {
    ui_->tautulliApiKeyLineEdit->setEchoMode(visible ? QLineEdit::Normal : QLineEdit::Password);
  });

  connect(ui_->tautulliTestButton, &QPushButton::clicked, this, [this] {
    const std::string host = ui_->tautulliHostLineEdit->text().toStdString();
    const std::string apiKey = ui_->tautulliApiKeyLineEdit->text().toStdString();

    ui_->tautulliTestButton->setEnabled(false);
    ui_->tautulliStatusLabel->setStyleSheet(QString());
    ui_->tautulliStatusLabel->setText("Testing connection...");

    // The page is passed as the request context, so a pending reply is
    // cancelled when the dialog is closed.
    track::tautulli::client()->testConnection(
        this, host, apiKey, [this](const bool success, const std::string& message) {
          auto label = ui_->tautulliStatusLabel;
          label->setText(QString::fromStdString(message));
          label->setStyleSheet(u"color: %1;"_s.arg(success ? theme.successColor().name()
                                                           : theme.errorColor().name()));
          ui_->tautulliTestButton->setEnabled(true);
        });
  });
}

void SettingsPagePlex::load() {
  ui_->tautulliEnabledCheckBox->setChecked(taiga::settings.tautulliEnabled());
  ui_->tautulliHostLineEdit->setText(
      QString::fromStdString(taiga::settings.tautulliHost()));
  ui_->tautulliApiKeyLineEdit->setText(
      QString::fromStdString(taiga::settings.tautulliApiKey()));
  ui_->tautulliApiKeyLineEdit->setEchoMode(QLineEdit::Password);
  ui_->tautulliApiKeyEchoCheckBox->setChecked(false);
  ui_->tautulliStatusLabel->setText(QString());
  ui_->tautulliStatusLabel->setStyleSheet(QString());
}

void SettingsPagePlex::apply() const {
  taiga::settings.setTautulliHost(ui_->tautulliHostLineEdit->text().toStdString());
  taiga::settings.setTautulliApiKey(ui_->tautulliApiKeyLineEdit->text().toStdString());

  // Must come after the host and API key are saved, since polling uses them.
  track::tautulli::client()->setEnabled(ui_->tautulliEnabledCheckBox->isChecked());
}

}  // namespace gui
