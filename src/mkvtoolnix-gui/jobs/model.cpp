#include "common/common_pch.h"

#include <QAbstractItemView>
#include <QMutexLocker>
#include <QSettings>
#include <QTimer>

#include "common/qt.h"
#include "mkvtoolnix-gui/jobs/model.h"
#include "mkvtoolnix-gui/jobs/mux_job.h"
#include "mkvtoolnix-gui/merge/mux_config.h"
#include "mkvtoolnix-gui/util/settings.h"
#include "mkvtoolnix-gui/util/util.h"

namespace mtx { namespace gui { namespace Jobs {

Model::Model(QObject *parent)
  : QStandardItemModel{parent}
  , m_mutex{QMutex::Recursive}
  , m_started{}
  , m_dontStartJobsNow{}
{
  auto labels = QStringList{};
  labels << QY("Description") << QY("Type") << QY("Status") << QY("Progress") << QY("Date added") << QY("Date started") << QY("Date finished");
  setHorizontalHeaderLabels(labels);

  horizontalHeaderItem(DescriptionColumn) ->setTextAlignment(Qt::AlignLeft);
  horizontalHeaderItem(ProgressColumn)    ->setTextAlignment(Qt::AlignRight);
  horizontalHeaderItem(DateAddedColumn)   ->setTextAlignment(Qt::AlignRight);
  horizontalHeaderItem(DateStartedColumn) ->setTextAlignment(Qt::AlignRight);
  horizontalHeaderItem(DateFinishedColumn)->setTextAlignment(Qt::AlignRight);
}

Model::~Model() {
}

QList<Job *>
Model::selectedJobs(QAbstractItemView *view)
  const {
  QList<Job *> jobs;
  Util::withSelectedIndexes(view, [&](QModelIndex const &idx) {
    jobs << m_jobsById[ data(idx, Util::JobIdRole).value<uint64_t>() ].get();
  });

  return jobs;
}

uint64_t
Model::idFromRow(int row)
  const {
  return item(row)->data(Util::JobIdRole).value<uint64_t>();
}

int
Model::rowFromId(uint64_t id)
  const {
  for (auto row = 0, numRows = rowCount(); row < numRows; ++row)
    if (idFromRow(row) == id)
      return row;
  return RowNotFound;
}

Job *
Model::fromId(uint64_t id)
  const {
  return m_jobsById.contains(id) ? m_jobsById[id].get() : nullptr;
}


bool
Model::hasJobs()
  const {
  return !!rowCount();
}

QList<QStandardItem *>
Model::createRow(Job const &job)
  const {
  auto items    = QList<QStandardItem *>{};
  auto progress = to_qs(boost::format("%1%%%") % job.m_progress);

  items << (new QStandardItem{job.m_description})                      << (new QStandardItem{job.displayableType()})                    << (new QStandardItem{Job::displayableStatus(job.m_status)}) << (new QStandardItem{progress})
        << (new QStandardItem{Util::displayableDate(job.m_dateAdded)}) << (new QStandardItem{Util::displayableDate(job.m_dateStarted)}) << (new QStandardItem{Util::displayableDate(job.m_dateFinished)});

  items[DescriptionColumn ]->setData(QVariant::fromValue(job.m_id), Util::JobIdRole);
  items[DescriptionColumn ]->setTextAlignment(Qt::AlignLeft);
  items[ProgressColumn    ]->setTextAlignment(Qt::AlignRight);
  items[DateAddedColumn   ]->setTextAlignment(Qt::AlignRight);
  items[DateStartedColumn ]->setTextAlignment(Qt::AlignRight);
  items[DateFinishedColumn]->setTextAlignment(Qt::AlignRight);

  return items;
}

void
Model::removeJobsIf(std::function<bool(Job const &)> predicate) {
  QMutexLocker locked{&m_mutex};

  auto toBeRemoved = QHash<Job const *, bool>{};

  for (auto row = rowCount(); 0 < row; --row) {
    auto job = m_jobsById[idFromRow(row - 1)].get();

    if (predicate(*job)) {
      m_jobsById.remove(job->m_id);
      toBeRemoved[job] = true;
      removeRow(row - 1);
    }
  }

  auto const keys = toBeRemoved.keys();
  for (auto const &job : keys)
    m_toBeProcessed.remove(job);

  updateProgress();
}

void
Model::add(JobPtr const &job) {
  QMutexLocker locked{&m_mutex};

  m_jobsById[job->m_id] = job;

  if (job->isToBeProcessed()) {
    m_toBeProcessed.insert(job.get());
    updateProgress();
  }

  invisibleRootItem()->appendRow(createRow(*job));

  connect(job.get(), SIGNAL(progressChanged(uint64_t,unsigned int)),              this, SLOT(onProgressChanged(uint64_t,unsigned int)));
  connect(job.get(), SIGNAL(statusChanged(uint64_t,mtx::gui::Jobs::Job::Status)), this, SLOT(onStatusChanged(uint64_t,mtx::gui::Jobs::Job::Status)));

  startNextAutoJob();
}

void
Model::onStatusChanged(uint64_t id,
                       Job::Status status) {
  QMutexLocker locked{&m_mutex};

  auto row = rowFromId(id);
  if (row == RowNotFound)
    return;

  auto const &job = *m_jobsById[id];
  auto numBefore  = m_toBeProcessed.count();

  if (job.isToBeProcessed())
    m_toBeProcessed.insert(&job);

  if (m_toBeProcessed.count() != numBefore)
    updateProgress();

  item(row, StatusColumn)->setText(Job::displayableStatus(job.m_status));

  if (Job::Running == status)
    item(row, DateStartedColumn)->setText(Util::displayableDate(job.m_dateStarted));

  else if ((Job::DoneOk == status) || (Job::DoneWarnings == status) || (Job::Failed == status) || (Job::Aborted == status))
    item(row, DateFinishedColumn)->setText(Util::displayableDate(job.m_dateFinished));

  startNextAutoJob();

  processAutomaticJobRemoval(id, status);
}

void
Model::removeScheduledJobs() {
  QMutexLocker locked{&m_mutex};

  removeJobsIf([this](Job const &job) { return m_toBeRemoved[job.m_id]; });
  m_toBeRemoved.clear();
}

void
Model::scheduleJobForRemoval(uint64_t id) {
  QMutexLocker locked{&m_mutex};

  m_toBeRemoved[id] = true;
  QTimer::singleShot(0, this, SLOT(removeScheduledJobs()));
}

void
Model::processAutomaticJobRemoval(uint64_t id,
                                  Job::Status status) {
  auto const &cfg = Util::Settings::get();
  if (cfg.m_jobRemovalPolicy == Util::Settings::JobRemovalPolicy::Never)
    return;

  bool doneOk       = Job::DoneOk       == status;
  bool doneWarnings = Job::DoneWarnings == status;
  bool done         = doneOk || doneWarnings || (Job::Failed == status) || (Job::Aborted == status);

  if (   ((cfg.m_jobRemovalPolicy == Util::Settings::JobRemovalPolicy::IfSuccessful)    && doneOk)
      || ((cfg.m_jobRemovalPolicy == Util::Settings::JobRemovalPolicy::IfWarningsFound) && doneWarnings)
      || ((cfg.m_jobRemovalPolicy == Util::Settings::JobRemovalPolicy::Always)          && done))
    scheduleJobForRemoval(id);
}

void
Model::onProgressChanged(uint64_t id,
                         unsigned int progress) {
  QMutexLocker locked{&m_mutex};

  auto row = rowFromId(id);
  if (row < rowCount()) {
    item(row, ProgressColumn)->setText(to_qs(boost::format("%1%%%") % progress));
    updateProgress();
  }
}

void
Model::startNextAutoJob() {
  if (m_dontStartJobsNow)
    return;

  QMutexLocker locked{&m_mutex};

  if (!m_started)
    return;

  Job *toStart = nullptr;
  for (auto row = 0, numRows = rowCount(); row < numRows; ++row) {
    auto job = m_jobsById[idFromRow(row)].get();

    if (Job::Running == job->m_status)
      return;
    if (!toStart && (Job::PendingAuto == job->m_status))
      toStart = job;
  }

  if (toStart) {
    toStart->start();
    return;
  }

  // All jobs are done. Clear total progress.
  m_toBeProcessed.clear();
  updateProgress();
}

void
Model::start() {
  m_started = true;
  startNextAutoJob();
}

void
Model::stop() {
  m_started = false;
}

void
Model::updateProgress() {
  QMutexLocker locked{&m_mutex};

  if (!m_toBeProcessed.count()) {
    emit progressChanged(0, 0);
    return;
  }

  auto numRunning      = 0u;
  auto numDone         = 0u;
  auto runningProgress = 0u;

  for (auto const &job : m_toBeProcessed)
    if (Job::Running == job->m_status) {
      ++numRunning;
      runningProgress += job->m_progress;

    } else if (!job->isToBeProcessed() && m_toBeProcessed.contains(job))
      ++numDone;

  auto progress      = numRunning ? runningProgress / numRunning : 0u;
  auto totalProgress = (numDone * 100 + runningProgress) / m_toBeProcessed.count();

  emit progressChanged(progress, totalProgress);
}

void
Model::saveJobs(QSettings &settings)
  const {
  settings.beginGroup("jobQueue");
  settings.setValue("numberOfJobs", rowCount());

  for (auto row = 0, numRows = rowCount(); row < numRows; ++row) {
    settings.beginGroup(Q("job %1").arg(row));
    m_jobsById[idFromRow(row)]->saveJob(settings);
    settings.endGroup();
  }

  settings.endGroup();
}

void
Model::loadJobs(QSettings &settings) {
  QMutexLocker locked{&m_mutex};

  m_dontStartJobsNow = true;

  m_jobsById.clear();
  removeRows(0, rowCount());

  settings.beginGroup("jobQueue");
  auto numberOfJobs = settings.value("numberOfJobs").toUInt();
  settings.endGroup();

  for (auto idx = 0u; idx < numberOfJobs; ++idx) {
    settings.beginGroup("jobQueue");
    settings.beginGroup(Q("job %1").arg(idx));

    try {
      add(Job::loadJob(settings));
    } catch (Merge::InvalidSettingsX &) {
    }

    while (!settings.group().isEmpty())
      settings.endGroup();
  }

  updateProgress();

  m_dontStartJobsNow = false;
}

Qt::DropActions
Model::supportedDropActions()
  const {
  return Qt::MoveAction;
}

Qt::ItemFlags
Model::flags(QModelIndex const &index)
  const {
  auto defaultFlags = QStandardItemModel::flags(index) & ~Qt::ItemIsDropEnabled;
  return index.isValid() ? defaultFlags | Qt::ItemIsDragEnabled : defaultFlags | Qt::ItemIsDropEnabled;
}

}}}
