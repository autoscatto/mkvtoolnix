#ifndef MTX_MKVTOOLNIX_GUI_HEADER_EDITOR_BIT_VALUE_PAGE_H
#define MTX_MKVTOOLNIX_GUI_HEADER_EDITOR_BIT_VALUE_PAGE_H

#include "common/common_pch.h"

#include "common/bitvalue.h"
#include "mkvtoolnix-gui/header_editor/value_page.h"

class QLineEdit;

namespace mtx { namespace gui { namespace HeaderEditor {

class Tab;

class BitValuePage: public ValuePage {
public:
  QLineEdit *m_leValue{};
  bitvalue_c m_originalValue;
  unsigned int m_bitLength;

public:
  BitValuePage(Tab &parent, PageBase &topLevelPage, EbmlMaster &master, EbmlCallbacks const &callbacks, translatable_string_c const &title, translatable_string_c const &description, unsigned int bitLength);
  virtual ~BitValuePage();

  virtual QWidget *createInputControl() override;
  virtual QString getOriginalValueAsString() const override;
  virtual QString getCurrentValueAsString() const override;
  virtual void resetValue() override;
  virtual bool validateValue() const override;
  virtual void copyValueToElement() override;
};

}}}

#endif  // MTX_MKVTOOLNIX_GUI_HEADER_EDITOR_BIT_VALUE_PAGE_H
