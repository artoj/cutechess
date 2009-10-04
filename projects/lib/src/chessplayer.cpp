/*
    This file is part of Cute Chess.

    Cute Chess is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Cute Chess is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Cute Chess.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "chessplayer.h"
#include "chessboard/chessboard.h"
#include <QString>
#include <QTimer>


ChessPlayer::ChessPlayer(QObject* parent)
	: QObject(parent),
	  m_state(NotStarted),
	  m_timer(0),
	  m_forfeited(false),
	  m_side(Chess::NoSide),
	  m_board(0),
	  m_opponent(0)
{
	m_timer = new QTimer(this);
	m_timer->setSingleShot(true);
	connect(m_timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
}

ChessPlayer::~ChessPlayer()
{
}

bool ChessPlayer::isReady() const
{
	switch (m_state)
	{
	case Idle:
	case Observing:
	case Thinking:
	case Disconnected:
		return true;
	default:
		return false;
	}
}

void ChessPlayer::newGame(Chess::Side side, ChessPlayer* opponent, Chess::Board* board)
{
	Q_ASSERT(opponent != 0);
	Q_ASSERT(board != 0);
	Q_ASSERT(isReady());

	m_forfeited = false;
	m_eval.clear();
	m_opponent = opponent;
	m_board = board;
	m_side = side;
	m_timeControl.setTimeLeft(m_timeControl.timePerTc());
	m_timeControl.setMovesLeft(m_timeControl.movesPerTc());

	m_state = Observing;
	startGame();
}

void ChessPlayer::endGame(Chess::Result result)
{
	Q_UNUSED(result);
	if (m_state != Observing && m_state != Thinking)
		return;

	m_state = FinishingGame;
	m_board = 0;
	m_timer->stop();
	disconnect(this, SIGNAL(ready()), this, SLOT(go()));
}

void ChessPlayer::go()
{
	m_state = Thinking;

	disconnect(this, SIGNAL(ready()), this, SLOT(go()));
	if (!isReady())
	{
		connect(this, SIGNAL(ready()), this, SLOT(go()));
		return;
	}

	Q_ASSERT(m_board != 0);
	m_side = m_board->sideToMove();
	
	startClock();
	startThinking();
}

void ChessPlayer::quit()
{
	m_state = Disconnected;
	emit disconnected();
}

const MoveEvaluation& ChessPlayer::evaluation() const
{
	return m_eval;
}

void ChessPlayer::startClock()
{
	if (m_state != Thinking)
		return;

	m_eval.clear();

	if (m_timeControl.timePerTc() != 0)
		emit startedThinking(m_timeControl.timeLeft());
	else if (m_timeControl.timePerMove() != 0)
		emit startedThinking(m_timeControl.timePerMove());
	
	m_timeControl.startTimer();
	m_timer->start(qMax(m_timeControl.timeLeft(), 0) + 200);
}

void ChessPlayer::makeBookMove(const Chess::Move& move)
{
	m_timeControl.startTimer();
	makeMove(move);
	m_timeControl.update();
}

const TimeControl* ChessPlayer::timeControl() const
{
	return &m_timeControl;
}

void ChessPlayer::setTimeControl(const TimeControl& timeControl)
{
	m_timeControl = timeControl;
}

Chess::Side ChessPlayer::side() const
{
	return m_side;
}

Chess::Board* ChessPlayer::board()
{
	return m_board;
}

const ChessPlayer* ChessPlayer::opponent() const
{
	return m_opponent;
}

ChessPlayer::State ChessPlayer::state() const
{
	return m_state;
}

void ChessPlayer::setState(State state)
{
	m_state = state;
}

QString ChessPlayer::name() const
{
	return m_name;
}

void ChessPlayer::setName(const QString& name)
{
	m_name = name;
}

void ChessPlayer::emitForfeit(Chess::Result::Code code, const QString& arg)
{
	if (m_forfeited)
		return;

	m_timer->stop();
	if (m_state == Thinking)
		m_state = Observing;
	m_forfeited = true;

	Q_ASSERT(m_side != Chess::NoSide);
	emit forfeit(Chess::Result(code, Chess::Side(!m_side), arg));
}

void ChessPlayer::emitMove(const Chess::Move& move)
{
	if (m_state == Thinking)
		m_state = Observing;

	m_timeControl.update();
	m_eval.setTime(m_timeControl.lastMoveTime());

	m_timer->stop();
	if (m_timeControl.timeLeft() <= 0)
	{
		emitForfeit(Chess::Result::WinByTimeout);
		return;
	}
	
	emit moveMade(move);
}

void ChessPlayer::closeConnection()
{
	m_state = Disconnected;
}

void ChessPlayer::onDisconnect()
{
	closeConnection();
	emitForfeit(Chess::Result::WinByDisconnection);
	emit disconnected();
}

void ChessPlayer::onTimeout()
{
	emitForfeit(Chess::Result::WinByTimeout);
}
