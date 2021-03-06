#ifndef MTX_MKVTOOLNIX_GUI_WATCH_JOBS_TAB_H
#define MTX_MKVTOOLNIX_GUI_WATCH_JOBS_TAB_H

#include "common/common_pch.h"

#include <QWidget>

#include "mkvtoolnix-gui/jobs/job.h"

namespace mtx { namespace gui { namespace WatchJobs {

namespace Ui {
class Tab;
}

class Tab : public QWidget {
  Q_OBJECT;

protected:
  // UI stuff:
  std::unique_ptr<Ui::Tab> ui;

public:
  explicit Tab(QWidget *parent);
  ~Tab();

  virtual void connectToJob(mtx::gui::Jobs::Job const &job);
  virtual void setInitialDisplay(mtx::gui::Jobs::Job const &job);

public slots:
  void onStatusChanged(uint64_t id, mtx::gui::Jobs::Job::Status status);
  void onProgressChanged(uint64_t id, unsigned int progress);
  void onLineRead(QString const &line, mtx::gui::Jobs::Job::LineType type);

  void onAbort();
  void onSaveOutput();

protected:
};

}}}

#endif // MTX_MKVTOOLNIX_GUI_WATCH_JOBS_TAB_H
