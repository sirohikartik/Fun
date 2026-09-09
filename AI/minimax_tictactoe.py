board = [['.' for _ in range(3)] for _ in range(3)]

total = {(i, j) for i in range(3) for j in range(3)}
taken = set()

ai = 'X'
me = 'O'


def print_board():
    for row in board:
        print(*row)


def endgame(i, j, board, no_won_rows, no_won_cols):

    symbol = board[i][j]

    if i not in no_won_rows:
        win = True

        for k in range(3):
            if board[i][k] != symbol:
                win = False
                break

        if win:
            return True

        symbols = {board[i][k] for k in range(3)}

        if 'X' in symbols and 'O' in symbols:
            no_won_rows.add(i)

    if j not in no_won_cols:
        win = True

        for k in range(3):
            if board[k][j] != symbol:
                win = False
                break

        if win:
            return True

        symbols = {board[k][j] for k in range(3)}

        if 'X' in symbols and 'O' in symbols:
            no_won_cols.add(j)

    if i == j:
        win = True

        for k in range(3):
            if board[k][k] != symbol:
                win = False
                break

        if win:
            return True

    if i + j == 2:
        win = True

        for k in range(3):
            if board[k][2 - k] != symbol:
                win = False
                break

        if win:
            return True

    return False


def score(player, no_won_rows, no_won_cols):

    moves = total.difference(taken)

    if not moves:
        return 0

    if player == ai:
        best = float('-inf')

        for move in moves:
            board[move[0]][move[1]] = ai
            taken.add(move)

            next_rows = no_won_rows.copy()
            next_cols = no_won_cols.copy()

            if endgame(move[0], move[1], board, next_rows, next_cols):
                result = 1
            else:
                result = score(me, next_rows, next_cols)

            board[move[0]][move[1]] = '.'
            taken.remove(move)

            best = max(best, result)

        return best

    else:
        best = float('inf')

        for move in moves:
            board[move[0]][move[1]] = me
            taken.add(move)

            next_rows = no_won_rows.copy()
            next_cols = no_won_cols.copy()

            if endgame(move[0], move[1], board, next_rows, next_cols):
                result = -1
            else:
                result = score(ai, next_rows, next_cols)

            board[move[0]][move[1]] = '.'
            taken.remove(move)

            best = min(best, result)

        return best


def simulation():

    global board, taken

    player = ai
    no_won_rows = set()
    no_won_cols = set()

    while True:

        moves = total.difference(taken)

        if not moves:
            print("DRAW")
            break

        best_score = float('-inf') if player == ai else float('inf')
        best_move = None

        for move in moves:

            board[move[0]][move[1]] = player
            taken.add(move)

            next_rows = no_won_rows.copy()
            next_cols = no_won_cols.copy()

            if endgame(move[0], move[1], board, next_rows, next_cols):
                result = 1 if player == ai else -1
            else:
                result = score(
                    me if player == ai else ai,
                    next_rows,
                    next_cols
                )

            board[move[0]][move[1]] = '.'
            taken.remove(move)

            if player == ai and result > best_score:
                best_score = result
                best_move = move

            elif player == me and result < best_score:
                best_score = result
                best_move = move

        i, j = best_move

        board[i][j] = player
        taken.add((i, j))

        endgame(i, j, board, no_won_rows, no_won_cols)

        print()
        print_board()

        if endgame(i, j, board, no_won_rows, no_won_cols):
            print(player, "wins!")
            break

        if len(taken) == 9:
            print("DRAW")
            break

        player = me if player == ai else ai


simulation()
