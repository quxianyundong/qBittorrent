/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez, Arnaud Demaiziere
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 *
 * Contact : chris@qbittorrent.org arnaud@qbittorrent.org
 */

#include <QDesktopServices>
#include <QInputDialog>
#include <QMenu>
#include <QStandardItemModel>
#include <QMessageBox>
#include <QString>
#include <QClipboard>

#include "rss_imp.h"
#include "FeedDownloader.h"
#include "bittorrent.h"

// display a right-click menu
void RSSImp::displayRSSListMenu(const QPoint& pos){
  if(!listStreams->indexAt(pos).isValid()) {
    // No item under the mouse, clear selection
    listStreams->clearSelection();
  }
  QMenu myFinishedListMenu(this);
  QList<QTreeWidgetItem*> selectedItems = listStreams->selectedItems();
  if(selectedItems.size() > 0) {
    myFinishedListMenu.addAction(actionUpdate_feed);
    myFinishedListMenu.addAction(actionMark_items_read);
    myFinishedListMenu.addSeparator();
    if(selectedItems.size() == 1) {
      myFinishedListMenu.addAction(actionRename_feed);
    }
    myFinishedListMenu.addAction(actionDelete_feed);
    myFinishedListMenu.addSeparator();
    myFinishedListMenu.addAction(actionCopy_feed_URL);
    if(selectedItems.size() == 1) {
      myFinishedListMenu.addSeparator();
      myFinishedListMenu.addAction(actionRSS_feed_downloader);
    }

  }else{
    myFinishedListMenu.addAction(actionNew_subscription);
    myFinishedListMenu.addAction(actionUpdate_all_feeds);
  }
  myFinishedListMenu.exec(QCursor::pos());
}

void RSSImp::displayItemsListMenu(const QPoint&){
  QMenu myItemListMenu(this);
  QList<QTreeWidgetItem*> selectedItems = listStreams->selectedItems();
  if(selectedItems.size() > 0) {
    myItemListMenu.addAction(actionDownload_torrent);
    myItemListMenu.addAction(actionOpen_news_URL);
  }
  myItemListMenu.exec(QCursor::pos());
}

QStringList RSSImp::getItemPath(QTreeWidgetItem *item) const {
  QStringList path;
  if(item) {
    if(item->parent()) {
      path = getItemPath(item->parent());
    }
    path << item->text(1);
  }
  return path;
}

QStringList RSSImp::getCurrentFeedPath() const {
  return getItemPath(listStreams->currentItem());
}

RssFile::FileType RSSImp::getItemType(QTreeWidgetItem *item) const {
  if(!item)
    return RssFile::FOLDER;
  return (RssFile::FileType)item->text(2).toInt();
}

// add a stream by a button
void RSSImp::on_newFeedButton_clicked() {
  QStringList dest_path;
  QTreeWidgetItem *current_item = listStreams->currentItem();
  if(getItemType(current_item) != RssFile::FOLDER)
    dest_path = getItemPath(current_item->parent());
  else
    dest_path = getItemPath(current_item);
  bool ok;
  QString clip_txt = qApp->clipboard()->text();
  QString default_url = "http://";
  if(clip_txt.startsWith("http://", Qt::CaseInsensitive) || clip_txt.startsWith("https://", Qt::CaseInsensitive) || clip_txt.startsWith("ftp://", Qt::CaseInsensitive)) {
    default_url = clip_txt;
  }
  QString newUrl = QInputDialog::getText(this, tr("Please type a rss stream url"), tr("Stream URL:"), QLineEdit::Normal, default_url, &ok);
  if(ok) {
    newUrl = newUrl.trimmed();
    if(!newUrl.isEmpty()){
      dest_path.append(newUrl);
      RssStream *stream = rssmanager->addStream(dest_path);
      if(stream == 0){
        // Already existing
        QMessageBox::warning(this, tr("qBittorrent"),
                             tr("This rss feed is already in the list."),
                             QMessageBox::Ok);
        return;
      }
      QTreeWidgetItem* item = new QTreeWidgetItem(listStreams);
      item->setText(0, stream->getName() + QString::fromUtf8("  (0)"));
      item->setText(1, stream->getUrl());
      item->setData(0,Qt::DecorationRole, QVariant(QIcon(":/Icons/loading.png")));
      if(listStreams->topLevelItemCount() == 1)
        selectFirstFeed();
      stream->refresh();
      rssmanager->saveStreamList();
    }
  }
}

// delete a stream by a button
void RSSImp::deleteSelectedItems() {
  QList<QTreeWidgetItem*> selectedItems = listStreams->selectedItems();
  if(selectedItems.size() == 0) return;
  if(!selectedItems.size()) return;
  int ret = QMessageBox::question(this, tr("Are you sure? -- qBittorrent"), tr("Are you sure you want to delete this RSS feed from the list?"),
                                  tr("&Yes"), tr("&No"),
                                  QString(), 0, 1);
  if(!ret) {
    foreach(QTreeWidgetItem *item, selectedItems){
      if(listStreams->currentItem() == item){
        textBrowser->clear();
        listNews->clear();
      }
      rssmanager->removeFile(getItemPath(item));
      delete item;
    }
    rssmanager->saveStreamList();
  }
}

// refresh all streams by a button
void RSSImp::on_updateAllButton_clicked() {
  unsigned int nbFeeds = listStreams->topLevelItemCount();
  for(unsigned int i=0; i<nbFeeds; ++i)
    listStreams->topLevelItem(i)->setData(0,Qt::DecorationRole, QVariant(QIcon(":/Icons/loading.png")));
  rssmanager->refreshAll();
}

void RSSImp::downloadTorrent() {
  QList<QListWidgetItem *> selected_items = listNews->selectedItems();
  foreach(const QListWidgetItem* item, selected_items) {
    RssItem* news =  ((RssStream*)rssmanager->getFile(getCurrentFeedPath()))->getItem(listNews->row(item));
    BTSession->downloadFromUrl(news->getTorrentUrl());
  }
}

// open the url of the news in a browser
void RSSImp::openNewsUrl() {
  QList<QListWidgetItem *> selected_items = listNews->selectedItems();
  foreach(const QListWidgetItem* item, selected_items) {
    RssItem* news =  ((RssStream*)rssmanager->getFile(getCurrentFeedPath()))->getItem(listNews->row(item));
    QString link = news->getLink();
    if(!link.isEmpty())
      QDesktopServices::openUrl(QUrl(link));
  }
}

//right-click on stream : give him an alias
void RSSImp::renameFiles() {
  QList<QTreeWidgetItem*> selectedItems = listStreams->selectedItems();
  Q_ASSERT(selectedItems.size() == 1);
  QTreeWidgetItem *item = selectedItems.at(0);
  bool ok;
  QString newName = QInputDialog::getText(this, tr("Please choose a new name for this RSS feed"), tr("New feed name:"), QLineEdit::Normal, rssmanager->getFile(getItemPath(item))->getName(), &ok);
  if(ok) {
    rssmanager->rename(getItemPath(item), newName);
  }
}

//right-click on stream : refresh it
void RSSImp::refreshSelectedStreams() {
  QList<QTreeWidgetItem*> selectedItems = listStreams->selectedItems();
  QTreeWidgetItem* item;
  foreach(item, selectedItems){
    rssmanager->refresh(getItemPath(item));
    if(getItemType(item) == RssFile::STREAM)
      item->setData(0,Qt::DecorationRole, QVariant(QIcon(":/Icons/loading.png")));
  }
}

void RSSImp::copySelectedFeedsURL() {
  QStringList URLs;
  QList<QTreeWidgetItem*> selectedItems = listStreams->selectedItems();
  QTreeWidgetItem* item;
  foreach(item, selectedItems){
    URLs << item->text(1);
  }
  qApp->clipboard()->setText(URLs.join("\n"));
}

void RSSImp::showFeedDownloader() {
  QTreeWidgetItem* item = listStreams->selectedItems()[0];
  RssFile* rss_item = rssmanager->getFile(getItemPath(item));
  if(rss_item->getType() == RssFile::STREAM)
    new FeedDownloaderDlg(this, item->text(1), rss_item->getName(), BTSession);
}

void RSSImp::on_markReadButton_clicked() {
  QList<QTreeWidgetItem*> selectedItems = listStreams->selectedItems();
  QTreeWidgetItem* item;
  foreach(item, selectedItems){
    RssFile *rss_item = rssmanager->getFile(getItemPath(item));
    rss_item->markAllAsRead();
    item->setData(0, Qt::DisplayRole, rss_item->getName()+ QString::fromUtf8("  (0)"));
  }
  if(selectedItems.size())
    refreshNewsList(listStreams->currentItem());
}

void RSSImp::fillFeedsList(QTreeWidgetItem *parent, RssFolder *rss_parent) {
  QList<RssFile*> children;
  if(parent) {
    children = rss_parent->getContent();
  } else {
    children = rssmanager->getContent();
  }
  foreach(RssFile* rss_child, children){
    QTreeWidgetItem* item;
    if(!parent)
      item = new QTreeWidgetItem(listStreams);
    else
      item = new QTreeWidgetItem(parent);
    item->setData(0, Qt::DisplayRole, rss_child->getName()+ QString::fromUtf8("  (")+QString::number(rss_child->getNbUnRead(), 10)+QString(")"));
    if(rss_child->getType() == RssFile::STREAM) {
      item->setData(0,Qt::DecorationRole, QVariant(QIcon(QString::fromUtf8(":/Icons/loading.png"))));
      item->setData(1, Qt::DisplayRole, ((RssStream*)rss_child)->getUrl());
      item->setData(2, Qt::DisplayRole, QVariant((int)rss_child->getType()));
    } else {
      item->setData(0,Qt::DecorationRole, QVariant(QIcon(QString::fromUtf8(":/Icons/oxygen/folder.png"))));
      item->setData(1, Qt::DisplayRole, ((RssFolder*)rss_child)->getName());
      item->setData(2, Qt::DisplayRole, QVariant((int)rss_child->getType()));
      // Recurvive call to load sub folders/files
      fillFeedsList(item, (RssFolder*)rss_child);
    }
  }
}

// fills the newsList
void RSSImp::refreshNewsList(QTreeWidgetItem* item) {
  if(!item) {
    listNews->clear();
    return;
  }
  RssStream *stream = (RssStream*)rssmanager->getFile(getCurrentFeedPath());
  qDebug("Getting the list of news");
  QList<RssItem*> news = stream->getNewsList();
  // Clear the list first
  textBrowser->clear();
  listNews->clear();
  qDebug("Got the list of news");
  foreach(RssItem* article, news){
    QListWidgetItem* it = new QListWidgetItem(article->getTitle(), listNews);
    if(article->isRead()){
      it->setData(Qt::ForegroundRole, QVariant(QColor("grey")));
      it->setData(Qt::DecorationRole, QVariant(QIcon(":/Icons/sphere.png")));
    }else{
      it->setData(Qt::ForegroundRole, QVariant(QColor("blue")));
      it->setData(Qt::DecorationRole, QVariant(QIcon(":/Icons/sphere2.png")));
    }
  }
  qDebug("Added all news to the GUI");
  //selectFirstNews();
  qDebug("First news selected");
}

// display a news
void RSSImp::refreshTextBrowser(QListWidgetItem *item) {
  if(!item) return;
  RssStream *stream = (RssStream*)rssmanager->getFile(getCurrentFeedPath());
  RssItem* article = stream->getItem(listNews->row(item));
  QString html;
  html += "<div style='border: 2px solid red; margin-left: 5px; margin-right: 5px; margin-bottom: 5px;'>";
  html += "<div style='background-color: #678db2; font-weight: bold; color: #fff;'>"+article->getTitle() + "</div>";
  if(article->getDate().isValid()) {
    html += "<div style='background-color: #efefef;'><b>"+tr("Date: ")+"</b>"+article->getDate().toString()+"</div>";
  }
  if(!article->getAuthor().isEmpty()) {
    html += "<div style='background-color: #efefef;'><b>"+tr("Author: ")+"</b>"+article->getAuthor()+"</div>";
  }
  html += "</div>";
  html += "<divstyle='margin-left: 5px; margin-right: 5px;'>"+article->getDescription()+"</div>";
  textBrowser->setHtml(html);
  article->setRead();
  item->setData(Qt::ForegroundRole, QVariant(QColor("grey")));
  item->setData(Qt::DecorationRole, QVariant(QIcon(":/Icons/sphere.png")));
  updateFeedNbNews(stream);
}

void RSSImp::saveSlidersPosition() {
  // Remember sliders positions
  QSettings settings("qBittorrent", "qBittorrent");
  settings.setValue("rss/splitter_h", splitter_h->saveState());
  settings.setValue("rss/splitter_v", splitter_v->saveState());
  qDebug("Splitters position saved");
}

void RSSImp::restoreSlidersPosition() {
  QSettings settings("qBittorrent", "qBittorrent");
  QByteArray pos_h = settings.value("rss/splitter_h", QByteArray()).toByteArray();
  if(!pos_h.isNull()) {
    splitter_h->restoreState(pos_h);
  }
  QByteArray pos_v = settings.value("rss/splitter_v", QByteArray()).toByteArray();
  if(!pos_v.isNull()) {
    splitter_v->restoreState(pos_v);
  }
}

QTreeWidgetItem* RSSImp::getTreeItemFromUrl(QString url) const{
  QList<QTreeWidgetItem*> items = listStreams->findItems(url, Qt::MatchExactly, 1);
  Q_ASSERT(items.size() == 1);
  return items.at(0);
}

void RSSImp::updateFeedIcon(QString url, QString icon_path){
  QTreeWidgetItem *item = getTreeItemFromUrl(url);
  item->setData(0,Qt::DecorationRole, QVariant(QIcon(icon_path)));
}

void RSSImp::updateFeedNbNews(RssStream* stream){
  QTreeWidgetItem *item = getTreeItemFromUrl(stream->getUrl());
  item->setText(0, stream->getName() + QString::fromUtf8("  (") + QString::number(stream->getNbUnRead(), 10)+ QString(")"));
}

QString RSSImp::getCurrentFeedUrl() const {
  QTreeWidgetItem *item = listStreams->currentItem();
  if(item)
    return item->text(1);
  return QString::null;
}

void RSSImp::updateFeedInfos(QString url, QString aliasOrUrl, unsigned int nbUnread){
  QTreeWidgetItem *item = getTreeItemFromUrl(url);
  RssStream *stream = (RssStream*)rssmanager->getFile(getItemPath(item));
  item->setText(0, aliasOrUrl + QString::fromUtf8("  (") + QString::number(nbUnread, 10)+ QString(")"));
  item->setData(0,Qt::DecorationRole, QVariant(QIcon(stream->getIconPath())));
  // If the feed is selected, update the displayed news
  if(listStreams->currentItem() == item){
    refreshNewsList(item);
  }
}

RSSImp::RSSImp(bittorrent *BTSession) : QWidget(), BTSession(BTSession){
  setupUi(this);

  // Hide second column (url) and third column (type)
  listStreams->hideColumn(1);
  listStreams->hideColumn(2);

  rssmanager = new RssManager(BTSession);
  fillFeedsList();
  connect(rssmanager, SIGNAL(feedInfosChanged(QString, QString, unsigned int)), this, SLOT(updateFeedInfos(QString, QString, unsigned int)));
  connect(rssmanager, SIGNAL(feedIconChanged(QString, QString)), this, SLOT(updateFeedIcon(QString, QString)));

  connect(listStreams, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayRSSListMenu(const QPoint&)));
  connect(listNews, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayItemsListMenu(const QPoint&)));

  // Feeds list actions
  connect(actionDelete_feed, SIGNAL(triggered()), this, SLOT(deleteSelectedItems()));
  connect(actionRename_feed, SIGNAL(triggered()), this, SLOT(renameFiles()));
  connect(actionUpdate_feed, SIGNAL(triggered()), this, SLOT(refreshSelectedStreams()));
  connect(actionNew_subscription, SIGNAL(triggered()), this, SLOT(on_newFeedButton_clicked()));
  connect(actionUpdate_all_feeds, SIGNAL(triggered()), this, SLOT(on_updateAllButton_clicked()));
  connect(actionCopy_feed_URL, SIGNAL(triggered()), this, SLOT(copySelectedFeedsURL()));
  connect(actionRSS_feed_downloader, SIGNAL(triggered()), this, SLOT(showFeedDownloader()));
  connect(actionMark_items_read, SIGNAL(triggered()), this, SLOT(on_markReadButton_clicked()));
  // News list actions
  connect(actionOpen_news_URL, SIGNAL(triggered()), this, SLOT(openNewsUrl()));
  connect(actionDownload_torrent, SIGNAL(triggered()), this, SLOT(downloadTorrent()));

  connect(listStreams, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)), this, SLOT(refreshNewsList(QTreeWidgetItem*)));
  connect(listNews, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(refreshTextBrowser(QListWidgetItem *)));
  connect(listNews, SIGNAL(itemDoubleClicked(QListWidgetItem *)), this, SLOT(downloadTorrent()));

  // Select first news of first feed
  selectFirstFeed();
  // Refresh all feeds
  rssmanager->refreshAll();
  // Restore sliders position
  restoreSlidersPosition();
  // Bind saveSliders slots
  connect(splitter_v, SIGNAL(splitterMoved(int, int)), this, SLOT(saveSlidersPosition()));
  connect(splitter_h, SIGNAL(splitterMoved(int, int)), this, SLOT(saveSlidersPosition()));
  qDebug("RSSImp constructed");
}

void RSSImp::selectFirstFeed(){
  if(listStreams->topLevelItemCount()){
    QTreeWidgetItem *first = listStreams->topLevelItem(0);
    listStreams->setCurrentItem(first);
  }
}

RSSImp::~RSSImp(){
  qDebug("Deleting RSSImp...");
  delete rssmanager;
  qDebug("RSSImp deleted");
}

