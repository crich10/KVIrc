//=============================================================================
//
//   File : KviWindowListBase.cpp
//   Creation date : Thu Jan 7 1999 03:59:43 CEST by Szymon Stefanek
//
//   This file is part of the KVirc irc client distribution
//   Copyright (C) 1999-2010 Szymon Stefanek (pragma at kvirc dot net)
//
//   This program is FREE software. You can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your opinion) any later version.
//
//   This program is distributed in the HOPE that it will be USEFUL,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//   See the GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program. If not, write to the Free Software Foundation,
//   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//=============================================================================


//#define KVI_WINDOWLISTBUTTON_MIN_WIDTH 100
#define KVI_WINDOWLISTBUTTON_CONTEXTINDICATORHEIGHT 4
#define KVI_WINDOWLISTBUTTON_CONTEXTINDICATORSPACING 1
#define KVI_WINDOWLISTBUTTON_TOP_MARGIN 6
#define KVI_WINDOWLISTBUTTON_LEFT_MARGIN 6
#define KVI_WINDOWLISTBUTTON_BOTTOM_MARGIN 6
#define KVI_WINDOWLISTBUTTON_RIGHT_MARGIN 6
#define KVI_WINDOWLIST_MIN_WIDTH 120

#define _KVI_DEBUG_CHECK_RANGE_
#include "KviChannelWindow.h"
#include "KviConsoleWindow.h"
#include "kvi_debug.h"
#include "KviDynamicToolTip.h"
#include "KviMainWindow.h"
#include "KviIrcConnection.h"
#include "KviLocale.h"
#include "KviOptions.h"
#include "kvi_settings.h"
#include "KviCString.h"
#include "KviWindow.h"
#include "KviWindowListBase.h"


// FIXME: #warning "The tree WindowList min width should be configurable"
#include <QFontMetrics>
#include <QPainter>
#include <QPixmap>
#include <QImage>
#include <QCursor>
#include <QPaintEvent>
#include <QEvent>

extern QPixmap * g_pActivityMeterPixmap;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// KviWindowListBase
//

KviWindowListBase::KviWindowListBase()
: QDockWidget(__tr2qs("Window List"),g_pFrame)
{
	// FIXME: this timer should be started only if KVI_OPTION_BOOL(KviOption_boolUseWindowListActivityMeter)
	setObjectName(__tr2qs("windowlist"));
	setFeatures(QDockWidget::DockWidgetMovable);

	// to hide the title bar completely must replace the default widget with a generic one
	m_pTitleWidget = new KviWindowListTitleWidget();
	setTitleBarWidget( m_pTitleWidget );

	m_pActivityMeterTimer = new QTimer();
	connect(m_pActivityMeterTimer,SIGNAL(timeout()),this,SLOT(updateActivityMeter()));
	connect(this, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)),this, SLOT(updateDockLocation(Qt::DockWidgetArea)));
	m_pActivityMeterTimer->start(5000);
}

KviWindowListBase::~KviWindowListBase()
{
	if(m_pTitleWidget)
		delete m_pTitleWidget;

	delete m_pActivityMeterTimer;
}

void KviWindowListBase::updateActivityMeter()
{

}

void KviWindowListBase::getTextForConsole(QString &szText,KviConsoleWindow * pConsole)
{
	if(pConsole->isConnected())
	{
		if(KVI_OPTION_BOOL(KviOption_boolShowNetworkNameForConsoleWindowListEntry))
		{
			// FIXME: Should never show "Standalone Servers" or "orphan_servers".
			//        It would also be nice to have a number appended to
			//        multiple entries with the same server name...but this costs too much.
			szText = pConsole->connection()->currentNetworkName();
			if(szText.isEmpty())
				szText = pConsole->connection()->currentServerName();
		} else {
			szText = pConsole->connection()->currentServerName();
		}
	} else {
		szText = pConsole->statusString();
	}
}


KviWindowListItem *  KviWindowListBase::item(int number)
{
	KviWindowListItem * it = firstItem();
	if(!setIterationPointer(it))return 0;

	while(it && (number > 0))
	{
		it = nextItem();
		number--;
	}
	return it;
}

void KviWindowListBase::switchWindow(bool bNext,bool bInContextOnly,bool bHighlightedOnly)
{
	if(!g_pActiveWindow)return;

	KviConsoleWindow * cons = g_pActiveWindow->console();
	KviWindowListItem * cur = g_pActiveWindow->windowListItem();
	KviWindowListItem * it = cur;

	if(!setIterationPointer(cur))return;

	while(it)
	{
		it = bNext ? nextItem() : prevItem();
		if(!it)it = bNext ? firstItem() : lastItem();
		if(it == cur)return; // did a complete loop.... no window to switch to
		if(it)
		{
			if(bHighlightedOnly && it->highlightLevel() < 3)
				continue;
			if(bInContextOnly && it->kviWindow()->console() != cons)
				continue;
			g_pFrame->setActiveWindow(it->kviWindow());
			return;
		}
	}
}

void KviWindowListBase::wheelEvent(QWheelEvent *e)
{
	if ( e->delta() > 0 )
		switchWindow(false, false);
	else
		switchWindow(true, false);
}

void KviWindowListBase::updateDockLocation(Qt::DockWidgetArea newArea)
{
	currentArea = newArea;
}

void KviWindowListBase::applyOptions()
{
	if(KVI_OPTION_BOOL(KviOption_boolShowTreeWindowListHeader))
	{
		m_pTitleWidget->setText(__tr2qs("Window List"));
		m_pTitleWidget->setMargin(2);
		m_pTitleWidget->setIndent(4);
	} else {
		m_pTitleWidget->setText("");
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// KviWindowListItem
//

KviWindowListItem::KviWindowListItem(KviWindow * wnd)
{
	m_pWindow = wnd;
	m_iHighlightLevel = 0;
	m_iProgress = -1;
}

KviWindowListItem::~KviWindowListItem()
{
	m_pWindow->m_pWindowListItem = 0;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// KviWindowListButton
//



KviWindowListButton::KviWindowListButton(QWidget * par,KviWindow * wnd,const char * name)
: QPushButton(par), KviWindowListItem(wnd)
{
	setObjectName(name);
	m_bActive         = false;
	m_pTip            = new KviDynamicToolTip(this);
	connect(m_pTip,SIGNAL(tipRequest(KviDynamicToolTip *,const QPoint &)),this,SLOT(tipRequest(KviDynamicToolTip *,const QPoint &)));
	setCheckable(true); //setToggleButton (true);
	//setFlat(KVI_OPTION_BOOL(KviOption_boolUseFlatClassicWindowListButtons)); // we paint it ourselves anyway
}

KviWindowListButton::~KviWindowListButton()
{
	delete m_pTip; //not necessary ?
}

void KviWindowListButton::tipRequest(KviDynamicToolTip *,const QPoint &)
{
	if(KVI_OPTION_BOOL(KviOption_boolShowWindowListToolTips))
	{
		QString szText;
		m_pWindow->getWindowListTipText(szText);
		m_pTip->tip(rect(),szText);
	}
}

void KviWindowListButton::mousePressEvent(QMouseEvent *e)
{
	if(e->button() & Qt::LeftButton)
	{
		if(e->modifiers() & Qt::ShiftModifier)
		{
			m_pWindow->delayedClose();
		} else {
			if((g_pActiveWindow != m_pWindow)  || (m_pWindow->isMinimized()))g_pFrame->setActiveWindow(m_pWindow);
			else m_pWindow->minimize();
		}
	} else m_pWindow->contextPopup();
}

void KviWindowListButton::contextMenuEvent(QContextMenuEvent *e)
{
	m_pWindow->contextPopup();
	e->accept();
}

void KviWindowListButton::setActive(bool bActive)
{
	if(bActive)
	{
		m_bActive         = true;
		m_iHighlightLevel = 0;
	} else {
		if(m_bActive)
		{
			// was active... unset the highlighting if it was silently turned on while being active...
			m_iHighlightLevel = 0;
		}
		m_bActive      = false;
	}
	setChecked(bActive);
	update();
}

void KviWindowListButton::paintEvent(QPaintEvent *)
{
	QPainter p(this);

	QStyleOptionButton opt;
	opt.initFrom(this);

	if(isChecked())
		opt.state = QStyle::State_On | QStyle::State_Active;

	if(KVI_OPTION_BOOL(KviOption_boolUseFlatClassicWindowListButtons))
		opt.features |= QStyleOptionButton::Flat;

	//style()->drawPrimitive(QStyle::PE_PanelButtonCommand,&opt,&p,this);

	style()->drawControl(QStyle::CE_PushButtonBevel,&opt,&p,this);

	drawButtonLabel(&p);
}

void KviWindowListButton::drawButtonLabel(QPainter * pPainter)
{
	int iHeight = height();
	int iWidth = width();

	if(KVI_OPTION_BOOL(KviOption_boolUseWindowListIrcContextIndicator))
	{
		iHeight -= KVI_WINDOWLISTBUTTON_CONTEXTINDICATORHEIGHT;
		QColor base = palette().color(QPalette::Background);
		if(m_pWindow->console())
		{
			QColor cntx = KVI_OPTION_ICCOLOR(m_pWindow->console()->context()->id() % KVI_NUM_ICCOLOR_OPTIONS);
			base.setRgb(
					(base.red() + cntx.red()) >> 1,
					(base.green() + cntx.green()) >> 1,
					(base.blue() + cntx.blue()) >> 1
				);

			pPainter->fillRect(
					KVI_WINDOWLISTBUTTON_LEFT_MARGIN,
					iHeight - KVI_WINDOWLISTBUTTON_BOTTOM_MARGIN,
					iWidth - (KVI_WINDOWLISTBUTTON_LEFT_MARGIN + KVI_WINDOWLISTBUTTON_RIGHT_MARGIN),
					KVI_WINDOWLISTBUTTON_CONTEXTINDICATORHEIGHT,
					base
				);
		} else {
			pPainter->fillRect(
					KVI_WINDOWLISTBUTTON_LEFT_MARGIN,
					iHeight - KVI_WINDOWLISTBUTTON_BOTTOM_MARGIN,
					iWidth - (KVI_WINDOWLISTBUTTON_LEFT_MARGIN + KVI_WINDOWLISTBUTTON_RIGHT_MARGIN),
					KVI_WINDOWLISTBUTTON_CONTEXTINDICATORHEIGHT,
					palette().brush(QPalette::Background)
				);
		}
		iHeight -= KVI_WINDOWLISTBUTTON_CONTEXTINDICATORSPACING;
	}

	int daX = KVI_WINDOWLISTBUTTON_LEFT_MARGIN;

	if(KVI_OPTION_BOOL(KviOption_boolUseWindowListIcons))
	{
		pPainter->drawPixmap(KVI_WINDOWLISTBUTTON_LEFT_MARGIN,KVI_WINDOWLISTBUTTON_TOP_MARGIN,*(m_pWindow->myIconPtr()));
		daX += 18;
	}

	if(KVI_OPTION_BOOL(KviOption_boolUseWindowListActivityMeter))
	{
		unsigned int uActivityValue;
		unsigned int uActivityTemperature;
		if(m_pWindow->activityMeter(&uActivityValue,&uActivityTemperature))
		{
			pPainter->drawPixmap(daX,KVI_WINDOWLISTBUTTON_TOP_MARGIN,*g_pActivityMeterPixmap,uActivityValue * 5,uActivityTemperature * 16,5,16);
			daX += 7;
		}
	}

	QRect cRect(
			daX,
			KVI_WINDOWLISTBUTTON_TOP_MARGIN,
			iWidth - (KVI_WINDOWLISTBUTTON_RIGHT_MARGIN + daX),
			iHeight - (KVI_WINDOWLISTBUTTON_TOP_MARGIN + KVI_WINDOWLISTBUTTON_BOTTOM_MARGIN)
		);

	if(m_iProgress >= 0)
	{
		// paint the progress bar
		int wdth = (m_iProgress * cRect.width()) / 100;
		pPainter->setPen(KVI_OPTION_COLOR(KviOption_colorWindowListProgressBar));
		pPainter->drawRect(cRect);
		pPainter->fillRect(daX,KVI_WINDOWLISTBUTTON_TOP_MARGIN,wdth,cRect.height(),KVI_OPTION_COLOR(KviOption_colorWindowListProgressBar));
	}

	QRect bRect;
	QString szText;

	bool bMinimized = m_pWindow->isMinimized();

	if(m_bActive)
	{
		pPainter->setPen(KVI_OPTION_COLOR(KviOption_colorWindowListNormalText));
	} else {
		int iLevel;

		switch(m_iHighlightLevel)
		{
			case 0: iLevel = bMinimized ? KviOption_colorWindowListMinimizedText : KviOption_colorWindowListNormalText; break;
			case 1: iLevel = KviOption_colorWindowListHighlight1Text; break;
			case 2: iLevel = KviOption_colorWindowListHighlight2Text; break;
			case 3: iLevel = KviOption_colorWindowListHighlight3Text; break;
			case 4: iLevel = KviOption_colorWindowListHighlight4Text; break;
			default: iLevel = KviOption_colorWindowListHighlight5Text; break;
		}
		pPainter->setPen(KVI_OPTION_COLOR(iLevel));
	}

	pPainter->setFont(KVI_OPTION_FONT(KviOption_fontWindowList));

	switch(m_pWindow->type())
	{
		case KVI_WINDOW_TYPE_CONSOLE:
		{
			QFont f = QFont(KVI_OPTION_FONT(KviOption_fontWindowList));
			f.setBold(true);
			pPainter->setFont(f);
			KviWindowListBase::getTextForConsole(szText,(KviConsoleWindow *)m_pWindow);
		}
		break;
		case KVI_WINDOW_TYPE_CHANNEL:
		case KVI_WINDOW_TYPE_DEADCHANNEL:
			szText = ((KviChannelWindow *)m_pWindow)->nameWithUserFlag();
		break;
		case KVI_WINDOW_TYPE_QUERY:
		case KVI_WINDOW_TYPE_DEADQUERY:
			szText = m_pWindow->windowName();
		break;
		default:
			szText = m_pWindow->plainTextCaption();
		break;
	}

	if(bMinimized)
		szText.prepend('(').append(')');

	pPainter->setClipRect(cRect);
	pPainter->drawText(cRect,Qt::AlignLeft | Qt::AlignTop,szText,&bRect);

	if(bRect.width() > cRect.width())
	{
		pPainter->setClipRect(cRect.right(),cRect.y(),10,cRect.height());
		QColor base = pPainter->pen().color();
		QColor bg   = palette().color(QPalette::Background);
		base.setRgb((base.red() + bg.red()) / 2,(base.green() + bg.green()) / 2,(base.blue() + bg.blue()) / 2);
		pPainter->setPen(base);
		cRect.setWidth(cRect.width() + 10);
		pPainter->drawText(cRect,Qt::AlignLeft | Qt::AlignTop,szText);
		pPainter->setClipRect(cRect.right(),cRect.y(),5,cRect.height());
		base.setRgb((base.red() + bg.red()) / 2,(base.green() + bg.green()) / 2,(base.blue() + bg.blue()) / 2);
		pPainter->setPen(base);
		cRect.setWidth(cRect.width() + 10);
		pPainter->drawText(cRect,Qt::AlignLeft | Qt::AlignTop,szText);
	}
}

void KviWindowListButton::captionChanged()
{
	update();
}

void KviWindowListButton::setProgress(int progress)
{
	if(progress == m_iProgress)
		return;
	m_iProgress = progress;
}

void KviWindowListButton::unhighlight()
{
	if(m_iHighlightLevel < 1)
		return;
	m_iHighlightLevel = 0;
	if(g_pFrame->dockExtension())
		g_pFrame->dockExtension()->refresh();
	update();
}

void KviWindowListButton::highlight(int iLevel)
{
	if(iLevel <= m_iHighlightLevel)
		return;
	if(m_bActive && g_pFrame->isActiveWindow())
		return;
	m_iHighlightLevel = iLevel;
	if(g_pFrame->dockExtension())
		g_pFrame->dockExtension()->refresh();
	update();
	if(m_bActive)
		return;
}


//
// KviClasicWindowList
//


KviClassicWindowList::KviClassicWindowList()
: KviWindowListBase()
{
	m_pButtonList = new KviPointerList<KviWindowListButton>;
	m_pButtonList->setAutoDelete(true);

	setFont(KVI_OPTION_FONT(KviOption_fontWindowList));
	calcButtonHeight();

	m_pBase = new QWidget(this);
	setWidget(m_pBase);

	m_pBase->setMinimumWidth(KVI_WINDOWLIST_MIN_WIDTH);
	m_pBase->setMinimumHeight(m_iButtonHeight);

	applyOptions();
	QTimer::singleShot( 0, this, SLOT(doLayout()));
}

KviClassicWindowList::~KviClassicWindowList()
{
	delete m_pButtonList;
	m_pButtonList = 0;
}

void KviClassicWindowList::orientationChangedSlot(Qt::Orientation)
{
	doLayout();
}

void KviClassicWindowList::updateActivityMeter()
{
	if(KVI_OPTION_BOOL(KviOption_boolUseWindowListActivityMeter))
	{
		for(KviWindowListButton * btn = m_pButtonList->first();btn;btn = m_pButtonList->next())
			btn->update();
	}
}

void KviClassicWindowList::calcButtonHeight()
{
	QFontMetrics fm(font());
	m_iButtonHeight = fm.lineSpacing();
	if(m_iButtonHeight < 16)
		m_iButtonHeight = 16; // pixmap size
	m_iButtonHeight += KVI_WINDOWLISTBUTTON_TOP_MARGIN + KVI_WINDOWLISTBUTTON_BOTTOM_MARGIN;
	if(KVI_OPTION_BOOL(KviOption_boolUseWindowListIrcContextIndicator))
		m_iButtonHeight += KVI_WINDOWLISTBUTTON_CONTEXTINDICATORHEIGHT + KVI_WINDOWLISTBUTTON_CONTEXTINDICATORSPACING;
}

void KviClassicWindowList::insertButton(KviWindowListButton * b)
{
	int idx = 0;
	// first sort by irc context
	for(KviWindowListButton * btn = m_pButtonList->first();btn;btn = m_pButtonList->next())
	{
		if(btn->kviWindow()->console() == b->kviWindow()->console())
		{
			// same irc context (or none)
			// sort by type now
			for(;btn;btn = m_pButtonList->next())
			{
				if(
						(btn->kviWindow()->type() > b->kviWindow()->type()) ||
						(btn->kviWindow()->console() != b->kviWindow()->console())
					)
				{
					// greater type or another irc context
					m_pButtonList->insert(idx,b);
					return;
				}
				
				if(btn->kviWindow()->type() == b->kviWindow()->type())
				{
					// same type!
					// sort by name
					if(
							KVI_OPTION_BOOL(KviOption_boolSortWindowListItemsByName) &&
							(KviQString::cmpCI(btn->kviWindow()->windowName(),b->kviWindow()->windowName()) > 0)
						)
					{
						// got a "higher one"
						m_pButtonList->insert(idx,b);
						return;
					}
				}
				idx++;
			}
			// ran out of buttons
			m_pButtonList->append(b);
			return;
		}
		
		if(!(btn->kviWindow()->console()) && b->kviWindow()->console())
		{
			// this must be a new console...insert before the contextless windows
			KVI_ASSERT(b->kviWindow()->console() == b->kviWindow());
			m_pButtonList->insert(idx,b);
			return;
		} else idx++; // wrong irc contet...go on searching
	}

	m_pButtonList->append(b);
}


KviWindowListItem * KviClassicWindowList::addItem(KviWindow * wnd)
{
	KviWindowListButton * b = new KviWindowListButton(m_pBase,wnd,"");
	insertButton(b);
	b->show();
	doLayout();
	if(g_pFrame->dockExtension())
		g_pFrame->dockExtension()->refresh();
/*	if(b->width() < m_pBase->width()) m_pBase->setMinimumWidth(b->width());
	if(b->height() < m_pBase->height()) m_pBase->setMinimumWidth(b->height());*/
	return b;
}

bool KviClassicWindowList::removeItem(KviWindowListItem * it)
{
	if(it)
	{
		m_pButtonList->removeRef((KviWindowListButton *)it);
		doLayout();
		if(g_pFrame->dockExtension())
			g_pFrame->dockExtension()->refresh();
	}
	return true;
}

void KviClassicWindowList::setActiveItem(KviWindowListItem * it)
{
	if(it)
	{
		for(KviWindowListButton * b = m_pButtonList->first();b;b = m_pButtonList->next())
		{
			b->setActive(((KviWindowListButton *)it) == b);
		}
		if(g_pFrame->dockExtension())
			g_pFrame->dockExtension()->refresh();
	}
}

void KviClassicWindowList::doLayout()
{
	if(!m_pButtonList->count())
		return;

	if(!m_pBase->isVisible())
	{
		// handle a windows bug: sometimes this ugly thing happens
		// this shouldn't hurt on other platforms
		if(isVisible())m_pBase->show();
	}

	int baseWidth = m_pBase->width();
	int btnsInRow = baseWidth / KVI_OPTION_UINT(KviOption_uintWindowListButtonMinWidth);
	int totCount = m_pButtonList->count();
	//int btnsInRow = totCount * totCount * btnWidth * btnWidth / (wdth * wdth);
	if(btnsInRow < 1)btnsInRow = 1;

	int rows = 0;
	while(totCount > 0)
	{
		rows++;
		totCount -= btnsInRow;
	}

	if(isFloating() || ((g_pFrame->dockWidgetArea(this) != Qt::BottomDockWidgetArea) && (g_pFrame->dockWidgetArea(this) != Qt::TopDockWidgetArea)))
	{
		QDockWidget::DockWidgetFeatures f = features();
		if(f & QDockWidget::DockWidgetVerticalTitleBar)
		{
			f &= ~QDockWidget::DockWidgetVerticalTitleBar;
			setFeatures(f);
		}
	} else {
		QDockWidget::DockWidgetFeatures f = features();
		if(!(f & QDockWidget::DockWidgetVerticalTitleBar))
		{
			f |= QDockWidget::DockWidgetVerticalTitleBar;
			setFeatures(f);
		}
	}

	if ((width() > height()) &&
		(rows > m_pBase->height() / m_iButtonHeight ))
	{
		rows = m_pBase->height() / m_iButtonHeight;
	}
	if(rows==0) rows=1;
	totCount = m_pButtonList->count();
	btnsInRow = totCount / rows;
	if(totCount % rows)btnsInRow++;

	int theWidth      = 0;
	int theX          = 0;
	int theY          = -m_iButtonHeight;
	int btnIdx        = 0;
	int btnInRow      = 1;
	for(KviWindowListButton * b = m_pButtonList->first();b;b = m_pButtonList->next())
	{
		if((btnIdx % btnsInRow) == 0)
		{
			int inRow = ((totCount < btnsInRow ) ? totCount : btnsInRow);
			theWidth = baseWidth / (inRow ? inRow : 1);
			theX     = 0;
			theY    += m_iButtonHeight;
			btnInRow = 1;
		} else {
			if((btnInRow == btnsInRow) || (totCount == 1))theWidth = baseWidth - theX;
		}

		if( KVI_OPTION_BOOL(KviOption_boolClassicWindowListSetMaximumButtonWidth) && ((unsigned int)theWidth > KVI_OPTION_UINT(KviOption_uintClassicWindowListMaximumButtonWidth)) && (width() > height())
			)
				theWidth = KVI_OPTION_UINT(KviOption_uintClassicWindowListMaximumButtonWidth);

		b->setGeometry(theX,theY,theWidth,m_iButtonHeight);

		if(btnInRow != btnsInRow)
		{
			theX += theWidth;
			btnInRow++;
		}
		btnIdx++;
		totCount--;
	}
}

void KviClassicWindowList::applyOptions()
{
	KviWindowListBase::applyOptions();

	calcButtonHeight();

	doLayout(); // this will trigger a repaint anyway
}

void KviClassicWindowList::resizeEvent(QResizeEvent *e)
{
	KviWindowListBase::resizeEvent(e);
	doLayout();
}

KviWindowListItem * KviClassicWindowList::firstItem()
{
	return m_pButtonList->first();
}

KviWindowListItem * KviClassicWindowList::lastItem(void)
{
	return m_pButtonList->last();
}

KviWindowListItem * KviClassicWindowList::nextItem()
{
	return m_pButtonList->next();
}

KviWindowListItem * KviClassicWindowList::prevItem(void)
{
	return m_pButtonList->prev();
}

bool KviClassicWindowList::setIterationPointer(KviWindowListItem * it)
{
	return (m_pButtonList->findRef((const KviWindowListButton *)it) != -1);
}


#ifndef COMPILE_USE_STANDALONE_MOC_SOURCES
#include "kvi_windowlist.moc"
#endif //!COMPILE_USE_STANDALONE_MOC_SOURCES