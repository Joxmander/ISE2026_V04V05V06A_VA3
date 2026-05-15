t <!DOCTYPE html>
t <html lang="es">
t <head>
t <meta charset="UTF-8">
t <meta name="viewport" content="width=device-width, initial-scale=1.0">
t <title>R.E.A.C.T. - Entrenamiento</title>
t <style>
t * { box-sizing: border-box; margin: 0; padding: 0; }
t body { font-family: -apple-system, "Segoe UI", Roboto, sans-serif;
t        background: linear-gradient(180deg, #0f172a 0%, #1e293b 100%);
t        min-height: 100vh; color: #f1f5f9; padding: 24px; }
t .wrap { max-width: 880px; margin: 0 auto; }
t .hero { background: rgba(255,255,255,0.05); backdrop-filter: blur(12px);
t         border: 1px solid rgba(255,255,255,0.1); border-radius: 20px;
t         padding: 32px; margin-bottom: 24px; text-align: center; }
t .hero .num { font-size: 0.85em; letter-spacing: 0.2em; color: #06b6d4;
t              margin-bottom: 8px; }
t .hero h1 { font-size: 2.2em; font-weight: 700; margin-bottom: 8px;
t            background: linear-gradient(90deg, #06b6d4, #f8fafc);
t            -webkit-background-clip: text; background-clip: text;
t            -webkit-text-fill-color: transparent; }
t .hero .jugador { font-size: 1.1em; color: #cbd5e1; }
t .hero .jugador strong { color: #fff; }
t .card { background: rgba(255,255,255,0.06); border-radius: 16px;
t         padding: 22px 24px; margin-bottom: 20px;
t         border: 1px solid rgba(255,255,255,0.08); }
t .card h2 { font-size: 1.1em; font-weight: 600; margin-bottom: 14px;
t            color: #f1f5f9; }
t .reglas { color: #cbd5e1; line-height: 1.7; }
t .reglas li { margin-bottom: 6px; margin-left: 22px; }
t .reglas strong { color: #06b6d4; }
t .estado-grid { display: grid; grid-template-columns: repeat(3, 1fr);
t                gap: 14px; }
t .stat { background: rgba(255,255,255,0.04); padding: 16px;
t         border-radius: 12px; text-align: center; }
t .stat .label { font-size: 0.75em; color: #94a3b8; letter-spacing: 0.1em;
t                text-transform: uppercase; }
t .stat .value { font-size: 2em; font-weight: 700; color: #06b6d4;
t                margin-top: 4px; font-variant-numeric: tabular-nums; }
t .barra-juego { width: 100%; height: 12px; border-radius: 6px;
t                background: rgba(255,255,255,0.08);
t                overflow: hidden; margin-top: 10px; }
t .barra-fill { height: 100%; width: 0%; transition: width 0.5s ease;
t               background: linear-gradient(90deg, #06b6d4, #2563eb); }
t .resultado { display: none; text-align: center; padding: 32px;
t              border: 1px solid rgba(16,185,129,0.3); border-radius: 16px;
t              background: linear-gradient(135deg, rgba(16,185,129,0.15),
t                                          rgba(6,182,212,0.15));
t              animation: aparecer 0.6s ease-out; }
t .resultado.fallo { border-color: rgba(239,68,68,0.3);
t                    background: linear-gradient(135deg,
t                                rgba(239,68,68,0.15),
t                                rgba(245,158,11,0.15)); }
t @keyframes aparecer { from { opacity: 0; transform: scale(0.95); }
t                       to   { opacity: 1; transform: scale(1); } }
t .resultado h2 { font-size: 1.8em; margin-bottom: 12px;
t                 background: linear-gradient(90deg, #10b981, #06b6d4);
t                 -webkit-background-clip: text; background-clip: text;
t                 -webkit-text-fill-color: transparent; }
t .resultado.fallo h2 { background: linear-gradient(90deg,#ef4444,#f59e0b);
t                       -webkit-background-clip: text;
t                       background-clip: text;
t                       -webkit-text-fill-color: transparent; }
t .resultado p { color: #cbd5e1; line-height: 1.6; margin-bottom: 16px; }
t .resultado .puntos { font-size: 3em; font-weight: 700; color: #f8fafc;
t                      margin: 16px 0; }
t .resultado .feedback { background: rgba(255,255,255,0.05);
t                        padding: 14px 18px; border-radius: 10px;
t                        margin-top: 16px; font-style: italic;
t                        color: #e2e8f0; }
t .btn-empezar { display: block; width: 100%; padding: 16px;
t                margin-top: 24px; color: #fff; font-size: 1.1em;
t                background: linear-gradient(135deg,#10b981 0%,#059669 100%);
t                font-weight: bold; border: none; border-radius: 12px;
t                cursor: pointer; transition: all 0.2s;
t                box-shadow: 0 6px 20px rgba(16,185,129,0.3); }
t .btn-empezar:hover { transform: translateY(-2px);
t                      box-shadow: 0 8px 24px rgba(16,185,129,0.4); }
t .volver { display: inline-block; margin-top: 20px; padding: 12px 28px;
t           background: rgba(255,255,255,0.08); color: #f1f5f9;
t           cursor: pointer; text-decoration: none; border-radius: 9px;
t           font-weight: 600; border: 1px solid rgba(255,255,255,0.15);
t           transition: all 0.2s; }
t .volver:hover { background: rgba(255,255,255,0.12);
t                 transform: translateY(-1px); }
t .indicador { display: inline-flex; align-items: center; gap: 8px;
t              padding: 6px 14px; background: rgba(6,182,212,0.15);
t              border: 1px solid rgba(6,182,212,0.3);
t              border-radius: 999px; font-size: 0.85em; color: #06b6d4;
t              margin-top: 8px; }
t .indicador.oculto { display: none; }
t .indicador::before { content: ""; width: 8px; height: 8px;
t                      border-radius: 50%; background: #06b6d4;
t                      animation: pulse 1.4s infinite; }
t @keyframes pulse { 0%,100% { opacity: 1; } 50% { opacity: 0.4; } }
t </style>
t </head>
t <body>
t <div class="wrap">
t   <section class="hero">
t     <div class="num" id="numModo">MODO -</div>
t     <h1 id="nombreModo">Cargando...</h1>
t     <div class="jugador">Atleta: <strong id="nombreJugador">-</strong></div>
t     <div class="indicador oculto" id="indicadorEstado">Esperando...</div>
t   </section>
t   <div id="faseInstrucciones">
t     <section class="card" id="cardReglas">
t       <h2>Instrucciones antes de empezar</h2>
t       <ol class="reglas" id="reglasLista"></ol>
t       <button class="btn-empezar" onclick="mandarSenalArranque()">
t         !ENTENDIDO, EMPEZAR AHORA!
t       </button>
t       <div style="text-align:center;">
t         <div class="volver" onclick="window.location.href='panel.cgi'">
t           Cancelar
t         </div>
t       </div>
t     </section>
t   </div>
t   <div id="faseVivo" style="display: none;">
t     <section class="card" id="cardEstado">
t       <h2>Estado en tiempo real</h2>
t       <div class="estado-grid">
t         <div class="stat">
t           <div class="label">Nivel</div>
t           <div class="value" id="statNivel">-</div>
t         </div>
t         <div class="stat">
t           <div class="label" id="statLabelB">Puntos</div>
t           <div class="value" id="statValorB">-</div>
t         </div>
t         <div class="stat">
t           <div class="label" id="statLabelC">Fallos</div>
t           <div class="value" id="statValorC">0</div>
t         </div>
t       </div>
t       <div class="barra-juego">
t         <div class="barra-fill" id="barraProgreso"></div>
t       </div>
t     </section>
t   </div>
t   <section class="resultado" id="cardResultado">
t     <h2 id="tituloResultado"></h2>
t     <p id="subtitResultado"></p>
t     <div class="puntos" id="puntosFinales"></div>
t     <div class="feedback" id="feedbackCognitivo"></div>
t     <div class="volver" onclick="window.location.href='panel.cgi'">
t       Volver al Panel
t     </div>
t   </section>
t </div>
t <script>
t const MODOS = {
t  1:{nombre:"Memoria",
t     reglas:["Simon Says cognitivo.","Repite secuencia."],
t     labelB:"Aciertos",labelC:"Fallos"},
t  2:{nombre:"Fuerza",
t     reglas:["Golpea con fuerza objetivo.","3 rondas."],
t     labelB:"Aciertos",labelC:"F.Media"},
t  3:{nombre:"Inhibicion",
t     reglas:["Solo si NO rojo + agudo.","3 fallos termina."],
t     labelB:"Nivel",labelC:"Fallos"},
t  4:{nombre:"Ritmo",
t     reglas:["Mantiene cadencia constante.","Cero errores."],
t     labelB:"Golpes",labelC:"Fallos"}
t };
t function fbC(modo, victoria, nivel, errores) {
t   if (modo == 1) return victoria ? "Excelente." : "Sigue entrenando.";
t   if (modo == 2) return victoria ? "Buen control." : "Modula fuerza.";
t   if (modo == 3) {
t     if(nivel>=3) return "Control top.";
t     return "Practica la atencion.";
t   }
t   if (modo == 4) return victoria ? "Timing top." : "Trabaja cadencia.";
t   return "";
t }
t const params = new URLSearchParams(window.location.search);
t const modoId = parseInt(params.get('modo') || '1');
t const nombre = params.get('nombre') || 'Invitado';
t const cfg    = MODOS[modoId] || MODOS[1];
t document.getElementById('numModo').textContent = 'MODO ' + modoId;
t document.getElementById('nombreModo').textContent = cfg.nombre;
t document.getElementById('nombreJugador').textContent = nombre;
t document.getElementById('statLabelB').textContent = cfg.labelB;
t document.getElementById('statLabelC').textContent = cfg.labelC;
t document.title = 'R.E.A.C.T. - ' + cfg.nombre;
t const ul = document.getElementById('reglasLista');
t cfg.reglas.forEach(r => {
t   const li = document.createElement('li');
t   li.innerHTML = r;
t   ul.appendChild(li);
t });
t let pInit = false; let pEnd = false;
t function mandarSenalArranque() {
t   document.getElementById('faseInstrucciones').style.display = 'none';
t   document.getElementById('faseVivo').style.display = 'block';
t   const ind = document.getElementById('indicadorEstado');
t   ind.classList.remove('oculto');
t   ind.textContent = 'Partida en curso...';
t   const data = new URLSearchParams();
t   data.append('jugador', nombre);
t   data.append('modo', modoId);
t   fetch('panel.cgi', { method:'POST', body: data });
t   pInit = true;
t }
t function mostrarResultado(modo, niv, cod) {
t   const r = document.getElementById('cardResultado');
t   document.getElementById('faseVivo').style.display = 'none';
t   r.style.display = 'block';
t   const vic = (modo == 3) ? (niv >= 3 && cod < 3) : (cod == 0);
t   r.classList.toggle('fallo', !vic);
t   const tRes = document.getElementById('tituloResultado');
t   tRes.textContent = vic ? '!Victoria!' : 'Partida terminada';
t   const stRes = document.getElementById('subtitResultado');
t   stRes.textContent = vic ? 'Completado.' : 'Intento guardado.';
t   const pts = (modo == 3) ? (3 - cod) + ' fallos' : 'Niv ' + niv;
t   document.getElementById('puntosFinales').textContent = pts;
t   const fb = document.getElementById('feedbackCognitivo');
t   fb.textContent = fbC(modo, vic, niv, cod);
t   document.getElementById('indicadorEstado').textContent = 'Finalizada';
t   pEnd = true;
t }
t function actualizarEstado(tramaTexto) {
t   const m = tramaTexto.match(/Niv\s*(\d+)\s*\|\s*Pts\s*(\d+)/i);
t   if (m) mostrarResultado(modoId, parseInt(m[1]), parseInt(m[2]));
t }
t setInterval(() => {
t   if (!pInit || pEnd) return;
t   fetch('panel.cgi').then(r => r.text()).then(html => {
t     const dom = new DOMParser().parseFromString(html, 'text/html');
t     const cTrama = dom.getElementById('caja_trama');
t     const trama = cTrama?.textContent.trim() || '';
t     if (trama) actualizarEstado(trama);
t   }).catch(() => {});
t }, 800);
t </script>
t </body>
t </html>
.