const VERTEX = `
attribute vec2 aPos;
void main() { gl_Position = vec4(aPos, 0.0, 1.0); }
`;

export function createShaderRenderer(canvas, fragmentSource)
{
    const gl = canvas.getContext('webgl');
    if (!gl) throw new Error('WebGL not supported');

    const program = link(gl, VERTEX, fragmentSource);
    gl.useProgram(program);

    const buffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
    gl.bufferData(gl.ARRAY_BUFFER,
        new Float32Array([-1, -1, 3, -1, -1, 3]), gl.STATIC_DRAW);

    const aPos = gl.getAttribLocation(program, 'aPos');
    gl.enableVertexAttribArray(aPos);
    gl.vertexAttribPointer(aPos, 2, gl.FLOAT, false, 0, 0);

    const uResolution = gl.getUniformLocation(program, 'uResolution');
    const uAngle = gl.getUniformLocation(program, 'uAngle');

    return {
        draw(angle)
        {
            gl.viewport(0, 0, canvas.width, canvas.height);
            gl.uniform2f(uResolution, canvas.width, canvas.height);
            gl.uniform1f(uAngle, angle);
            gl.drawArrays(gl.TRIANGLES, 0, 3);
        },
        dispose()
        {
            gl.deleteProgram(program);
            gl.deleteBuffer(buffer);
        },
    };
}

function link(gl, vertexSource, fragmentSource)
{
    const program = gl.createProgram();
    gl.attachShader(program, compile(gl, gl.VERTEX_SHADER, vertexSource));
    gl.attachShader(program, compile(gl, gl.FRAGMENT_SHADER, fragmentSource));
    gl.linkProgram(program);
    if (!gl.getProgramParameter(program, gl.LINK_STATUS))
        throw new Error(gl.getProgramInfoLog(program));
    return program;
}

function compile(gl, type, source)
{
    const shader = gl.createShader(type);
    gl.shaderSource(shader, source);
    gl.compileShader(shader);
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS))
        throw new Error(gl.getShaderInfoLog(shader));
    return shader;
}
