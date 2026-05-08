import { useEffect, useRef, useState } from 'react';

export function useNativeEvent(event, initial)
{
    const [value, setValue] = useState(initial);
    useEffect(() => window.eacp.on(event, setValue), [event]);
    return value;
}

export function useEventRate(event)
{
    const [hz, setHz] = useState(0);
    const count = useRef(0);

    useEffect(() =>
    {
        const unsub = window.eacp.on(event, () => { count.current++; });
        const interval = setInterval(() =>
        {
            setHz(count.current);
            count.current = 0;
        }, 1000);
        return () => { unsub(); clearInterval(interval); };
    }, [event]);

    return hz;
}
