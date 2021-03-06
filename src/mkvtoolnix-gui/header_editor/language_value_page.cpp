#include "common/common_pch.h"

#include <QAbstractItemView>
#include <QComboBox>

#include "common/qt.h"
#include "mkvtoolnix-gui/app.h"
#include "mkvtoolnix-gui/header_editor/language_value_page.h"
#include "mkvtoolnix-gui/util/util.h"

namespace mtx { namespace gui { namespace HeaderEditor {

using namespace mtx::gui;

LanguageValuePage::LanguageValuePage(Tab &parent,
                                     PageBase &topLevelPage,
                                     EbmlMaster &master,
                                     EbmlCallbacks const &callbacks,
                                     translatable_string_c const &title,
                                     translatable_string_c const &description)
  : ValuePage{parent, topLevelPage, master, callbacks, ValueType::AsciiString, title, description}
{
}

LanguageValuePage::~LanguageValuePage() {
}

QWidget *
LanguageValuePage::createInputControl() {
  if (m_element)
    m_originalValue = static_cast<EbmlString *>(m_element)->GetValue();

  m_cbValue = new QComboBox{this};
  m_cbValue->setFrame(true);
  m_cbValue->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);

  Util::setupLanguageComboBox(*m_cbValue, QStringList{} << Q(m_originalValue) << Q("und"));
  m_originalValueIdx = m_cbValue->currentIndex();

  return m_cbValue;
}

QString
LanguageValuePage::getOriginalValueAsString()
  const {
  return Q(m_originalValue);
}

QString
LanguageValuePage::getCurrentValueAsString()
  const {
  return m_cbValue->currentData().toString();
}

void
LanguageValuePage::resetValue() {
  m_cbValue->setCurrentIndex(m_originalValueIdx);
}

bool
LanguageValuePage::validateValue()
  const {
  return true;
}

void
LanguageValuePage::copyValueToElement() {
  static_cast<EbmlString *>(m_element)->SetValue(to_utf8(getCurrentValueAsString()));
}

}}}
