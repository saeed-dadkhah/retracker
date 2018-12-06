#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QMessageBox>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QInputDialog>
#include <QKeyEvent>

#include "UserPassDialog.h"
#include "TimePickerDialog.h"
#include "ApplyTimeToPaygirDialog.h"
#include "AddIssueDialog.h"
#include "commons.h"
#include "Settings.h"
#include "DatabaseManager.h"
#include "ProccessRunner.h"
#include "OnDestructionRunner.h"
#include "TimesliceEditorDialog.h"

MainWindow::MainWindow(QWidget *parent) :
QMainWindow(parent),
ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	connect(ui->wdg_issue_categories, &IssueCategoryManager::issue_selected, this, &MainWindow::on_issue_selected);
	connect(ui->wdg_issue_categories, &IssueCategoryManager::issue_edit_requested, this, &MainWindow::on_issue_edit_triggered);
	connect(ui->wdg_stop_watch, &StopWatchWidget::stopped, this, &MainWindow::on_stop_watch_stopped);
	connect(ui->wdg_stop_watch, &StopWatchWidget::dismissed, ui->wdg_today_summary, &TodaySummaryWidget::update);
	connect(&issue_manager, &IssueManager::issue_added, this, &MainWindow::on_issue_added);
	connect(&issue_manager, &IssueManager::issue_map_updated, ui->wdg_issue_categories, &IssueCategoryManager::update_issue_tables);
	connect(&issue_manager, &IssueManager::issue_updated, [this](){this->ui->wdg_today_summary->update();});

	ui->wdg_issue_categories->set_issue_manager(&issue_manager);
	ui->wdg_today_summary->set_issue_manager(&issue_manager);
	open_database();
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::login()
{
	UserPassDialog::get_credential();
}

void MainWindow::on_issue_selected(Issue::Id id)
{
	if (ui->wdg_stop_watch->is_active())
		return;

	if (!issue_manager.issue_exists(id))
	{
		QMessageBox::critical(this, "Errro", "Surprisingly issue doesn't exist. It definitely a bug!");
		return;
	}

	issue_manager.set_selected_issue_id(id);
	const Issue& current_issue = issue_manager.get_selected_issue();
	ui->wdg_stop_watch->set_current_issue(QString::number(current_issue.get_id()), current_issue.get_subject());
}

void MainWindow::on_btn_update_issues_clicked()
{
	OnDestructionRunner runner([this](){
		this->setCursor(Qt::ArrowCursor);
		this->ui->btn_update_issues->setEnabled(true);
	});

	Credential credit = UserPassDialog::get_credential();

	ui->btn_update_issues->setEnabled(false);
	setCursor(Qt::BusyCursor);
	if (!issue_manager.load_issues_from_peygir(credit.username, credit.password))
	{
		setCursor(Qt::ArrowCursor);
		QMessageBox::critical(this, "Load Issues", "Loading issues failed." + issue_manager.get_last_error());
		ui->btn_update_issues->setEnabled(true);
		return;
	}
	setCursor(Qt::ArrowCursor);

	DatabaseManager::instance().add_issues(issue_manager.get_issues());		// TODO: refactor
	ui->btn_update_issues->setEnabled(true);
}

void MainWindow::open_database()
{
	if (!DatabaseManager::instance().open_database("d:/retracker.db"))
	{
		QMessageBox::warning(this, "Database", "Couldn't open the database, " + DatabaseManager::instance().get_last_error());
		return;
	}

	ui->wdg_issue_categories->load_categories();
	IssueMap issues = DatabaseManager::instance().all_issues();
	issue_manager.set_issues(std::move(issues));
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_F && QApplication::keyboardModifiers() && Qt::ControlModifier)
		ui->wdg_issue_categories->focus();
	else
		QMainWindow::keyPressEvent(event);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
//	if (!DatabaseManager::instance().update_issues(issue_manager.get_issues()))
//		QMessageBox::critical(this, "Database", "Updating issues in database failed. " + DatabaseManager::instance().get_last_error());

	QMainWindow::closeEvent(event);
}

void MainWindow::on_stop_watch_stopped()
{
	issue_manager.add_timeslice_for_selected_issue(ui->wdg_stop_watch->get_timeslice());
}

void MainWindow::on_btn_apply_times_clicked()
{
	ApplyTimeToPaygirDialog dlg(issue_manager);
	dlg.exec();
}

void MainWindow::on_btn_add_issue_clicked()
{
	AddIssueDialog dlg;

	if (dlg.exec() == QDialog::Rejected)
		return;

	int issue_id = dlg.issue_id();
	if (dlg.create_new_issue())
	{
		issue_manager.add_issue(issue_id, dlg.get_subject(), static_cast<Issue::State>(dlg.get_state()));
		return;
	}

	OnDestructionRunner runner([this](){
		this->setCursor(Qt::ArrowCursor);
		this->ui->btn_add_issue->setEnabled(true);
	});

	Credential credit = UserPassDialog::get_credential();

	ui->btn_add_issue->setEnabled(false);
	setCursor(Qt::BusyCursor);
	bool loaded = issue_manager.load_issue_from_peygir(issue_id, credit.username, credit.password);
	if (!loaded)
	{
		QMessageBox::critical(this, "Load Issue", QString("Loading issue #%1 failed.").arg(issue_id));
		return;
	}

	ui->wdg_issue_categories->filter_issues(QString::number(issue_id));
}

void MainWindow::on_issue_added(Issue::Id id)
{
	DatabaseManager::instance().add_issue(issue_manager.get_issue_by_id(id));
	ui->wdg_issue_categories->add_issue(id);
}

void MainWindow::on_issue_edit_triggered(Issue::Id id)
{
	TimesliceEditorDialog dlg;
	dlg.set_issue_manager(&issue_manager);
	dlg.update_for_issue(id);

	dlg.exec();
}
