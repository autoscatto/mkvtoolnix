#ifndef MTX_MKVTOOLNIX_GUI_MUX_JOB_H
#define MTX_MKVTOOLNIX_GUI_MUX_JOB_H

#include "common/common_pch.h"

#include <QByteArray>
#include <QProcess>

#include "mkvtoolnix-gui/jobs/job.h"

class QTemporaryFile;

namespace mtx { namespace gui {

namespace Merge {

class MuxConfig;
using MuxConfigPtr = std::shared_ptr<MuxConfig>;

}

namespace Jobs {

class MuxJob: public Job {
  Q_OBJECT;
protected:
  mtx::gui::Merge::MuxConfigPtr m_config;
  QProcess m_process;
  bool m_aborted;
  QByteArray m_bytesRead;
  std::unique_ptr<QTemporaryFile> m_settingsFile;

public:
  MuxJob(Status status, mtx::gui::Merge::MuxConfigPtr const &config);
  virtual ~MuxJob();

  virtual void abort();
  virtual void start();

  virtual QString displayableType() const;
  virtual QString displayableDescription() const;

public slots:
  void readAvailable();
  void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
  void processError(QProcess::ProcessError error);

protected:
  void processBytesRead();
  void processLine(QString const &rawLine);
  virtual void saveJobInternal(QSettings &settings) const;

signals:
  void startedScanningPlaylists();
  void finishedScanningPlaylists();

public:
  static JobPtr loadMuxJob(QSettings &settings);
};

}}}

#endif // MTX_MKVTOOLNIX_GUI_MUX_JOB_H
