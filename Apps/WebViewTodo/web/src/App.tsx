import { useMemo, useState } from 'react';
import type { TodoItem } from './generated/schema';
import {
    addTodo,
    clearCompleted,
    editTodo,
    removeTodo,
    toggleTodo,
    useTodoState,
} from './state';

export default function App()
{
    console.log('render: App');
    const state = useTodoState();
    const [draft, setDraft] = useState('');

    const completedCount = useMemo(
        () => state.items.filter((item) => item.completed).length,
        [state.items]);
    const remaining = state.items.length - completedCount;

    const submitDraft = () =>
    {
        const text = draft.trim();
        if (text.length === 0) return;
        void addTodo(text);
        setDraft('');
    };

    return (
        <main>
            <header>
                <h1>todos</h1>
                <p className="sub">
                    State lives in C++ (<code>StateValue&lt;TodoState&gt;</code>) and is
                    broadcast over the WebView bridge. Every change you make round-trips
                    through a typed command and comes back as an event.
                </p>
            </header>

            <section className="card">
                <form
                    className="composer"
                    onSubmit={(event) =>
                    {
                        event.preventDefault();
                        submitDraft();
                    }}
                >
                    <input
                        type="text"
                        placeholder="What needs to be done?"
                        value={draft}
                        autoFocus
                        onChange={(event) => setDraft(event.target.value)}
                    />
                    <button type="submit" disabled={draft.trim().length === 0}>
                        Add
                    </button>
                </form>

                <ul className="list">
                    {state.items.map((item) => (
                        <TodoRow key={item.id} item={item} />
                    ))}
                    {state.items.length === 0 && (
                        <li className="empty">Nothing here yet — add a todo above.</li>
                    )}
                </ul>

                <footer className="footer">
                    <span className="count">
                        {remaining} remaining · {completedCount} done
                    </span>
                    <button
                        type="button"
                        className="link"
                        disabled={completedCount === 0}
                        onClick={() => void clearCompleted()}
                    >
                        Clear completed
                    </button>
                </footer>
            </section>
        </main>
    );
}

function TodoRow({ item }: { item: TodoItem })
{
    console.log('render: TodoRow', item.id);
    const [editing, setEditing] = useState(false);
    const [draft, setDraft] = useState(item.text);

    const beginEdit = () =>
    {
        setDraft(item.text);
        setEditing(true);
    };

    const commitEdit = () =>
    {
        const next = draft.trim();
        setEditing(false);
        if (next.length === 0) void removeTodo(item.id);
        else if (next !== item.text) void editTodo(item.id, next);
    };

    return (
        <li className={item.completed ? 'item done' : 'item'}>
            <input
                type="checkbox"
                checked={item.completed}
                onChange={() => void toggleTodo(item.id)}
            />
            {editing ? (
                <input
                    className="edit"
                    type="text"
                    value={draft}
                    autoFocus
                    onChange={(event) => setDraft(event.target.value)}
                    onBlur={commitEdit}
                    onKeyDown={(event) =>
                    {
                        if (event.key === 'Enter') commitEdit();
                        else if (event.key === 'Escape')
                        {
                            setDraft(item.text);
                            setEditing(false);
                        }
                    }}
                />
            ) : (
                <span className="text" onDoubleClick={beginEdit}>
                    {item.text}
                </span>
            )}
            <button
                type="button"
                className="remove"
                aria-label="Remove"
                onClick={() => void removeTodo(item.id)}
            >
                ×
            </button>
        </li>
    );
}
