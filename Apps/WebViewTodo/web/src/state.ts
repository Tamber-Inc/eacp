import { useEffect, useState } from 'react';
import { backend } from './generated/backend';
import type { TodoState } from './generated/schema';

const empty: TodoState = { items: [] };

export function useTodoState(): TodoState
{
    const [state, setState] = useState<TodoState>(empty);

    useEffect(() =>
    {
        let cancelled = false;

        void backend.getTodos().then((initial) =>
        {
            if (!cancelled)
                setState(initial);
        });

        const off = backend.on?.('todos', (payload) =>
            setState(payload as TodoState));

        return () =>
        {
            cancelled = true;
            off?.();
        };
    }, []);

    return state;
}

export const addTodo       = (text: string)                => backend.addTodo({ text });
export const toggleTodo    = (id: number)                  => backend.toggleTodo({ id });
export const editTodo      = (id: number, text: string)    => backend.editTodo({ id, text });
export const removeTodo    = (id: number)                  => backend.removeTodo({ id });
export const clearCompleted = ()                           => backend.clearCompleted();
