import { useEventRate, useNativeEvent } from './bridge';

const petals = [0, 60, 120, 180, 240, 300];

const layers = [
    { speed: 1.0, size: 320, hue: 0 },
    { speed: -1.7, size: 220, hue: 120 },
    { speed: 2.6, size: 130, hue: 240 },
];

export default function App()
{
    const { angle } = useNativeEvent('tick', { angle: 0 });
    const hz = useEventRate('tick');
    const pulse = 1 + 0.18 * Math.sin(angle * Math.PI / 60);

    return (
        <div className="stage">
            <h1>Native-driven React animation</h1>
            <p>The pinwheel angle is pushed from native ticks ({hz} Hz measured).</p>
            <div className="stack" style={{ transform: `scale(${pulse})` }}>
                {layers.map((layer, i) => (
                    <Wheel key={i} angle={angle * layer.speed} {...layer} />
                ))}
            </div>
        </div>
    );
}

function Wheel({ angle, size, hue })
{
    const style = {
        width: size,
        height: size,
        transform: `rotate(${angle}deg)`,
        filter: `hue-rotate(${hue}deg) `
              + `drop-shadow(0 0 14px hsla(${hue}, 80%, 60%, 0.65))`,
    };

    return (
        <svg viewBox="-100 -100 200 200" style={style}>
            {petals.map(rotation => <Petal key={rotation} rotation={rotation} />)}
        </svg>
    );
}

function Petal({ rotation })
{
    return (
        <path
            transform={`rotate(${rotation})`}
            d="M 0 0 L 50 -25 L 90 0 L 50 25 Z"
            fill={`hsl(${rotation}, 70%, 60%)`}
        />
    );
}
