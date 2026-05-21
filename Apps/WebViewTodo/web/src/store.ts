import { useMemo } from 'react';
import { backend } from './generated/backend';
import { makeKeyedStore } from './generated/react';

const todos = makeKeyedStore({
    backend,
    event: 'todos',
    fetch: backend.getTodos,
    initial: { items: [] },
    getItems: (s) => s.items,
    getKey:   (i) => i.id,
});

export const useTodos    = todos.useAll;
export const useTodoIds  = todos.useIds;
export const useTodoItem = todos.useItem;

export interface TodoSummary
{
    total: number;
    completed: number;
}

export function useTodoSummary(): TodoSummary
{
    const state = useTodos();

    return useMemo(() => ({
        total: state.items.length,
        completed: state.items.filter((i) => i.completed).length,
    }), [state]);
}

export const addTodo        = (text: string)             => backend.addTodo({ text });
export const toggleTodo     = (id: number)               => backend.toggleTodo({ id });
export const editTodo       = (id: number, text: string) => backend.editTodo({ id, text });
export const removeTodo     = (id: number)               => backend.removeTodo({ id });
export const clearCompleted = ()                         => backend.clearCompleted();
