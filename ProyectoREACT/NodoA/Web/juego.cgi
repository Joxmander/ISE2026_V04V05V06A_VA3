t <!DOCTYPE html>
t <html lang="es">
t <head>
t <meta charset="UTF-8">
t <meta name="viewport" content="width=device-width, initial-scale=1.0">
t <title>R.E.A.C.T. - Partida</title>
t <style>
t * { box-sizing: border-box; margin: 0; padding: 0; }
t :root { --c1: #0D9488; --c2: #10B981; }
t body[data-modo="1"] { --c1: #4F46E5; --c2: #7C3AED; }
t body[data-modo="2"] { --c1: #F59E0B; --c2: #DC2626; }
t body[data-modo="3"] { --c1: #DB2777; --c2: #9333EA; }
t body[data-modo="4"] { --c1: #0D9488; --c2: #10B981; }
t body {
t   font-family: -apple-system, sans-serif;
t   background: linear-gradient(180deg, #0f172a 0%, #1A3942 100%);
t   min-height: 100vh; color: #f1f5f9; padding: 24px;
t }
t .wrap { max-width: 920px; margin: 0 auto; }
t .hero {
t   background: linear-gradient(135deg, var(--c1), var(--c2));
t   border-radius: 20px; padding: 32px; margin-bottom: 24px; 
t   text-align: center; position: relative; overflow: hidden;
t }
t .hero img {
t   width: 80px; height: 80px; margin-bottom: 12px;
t   filter: brightness(0) invert(1);
t }
t .hero .num {
t   font-size: 0.85em; letter-spacing: 0.25em; margin-bottom: 6px;
t }
t .hero h1 {
t   font-size: 2.2em; font-weight: 700; margin-bottom: 6px;
t }
t .hero .subtit { font-size: 0.95em; font-style: italic; }
t .hero .jugador { font-size: 1.05em; margin-top: 14px; }
t .hero .jugador strong { color: #F4C842; }
t .indicador {
t   display: inline-flex; align-items: center; gap: 8px;
t   padding: 6px 14px; background: rgba(0,0,0,0.2);
t   border-radius: 99px; margin-top: 14px; font-size: 0.85em;
t }
t .indicador::before {
t   content: ""; width: 8px; height: 8px; border-radius: 50%;
t   background: #F4C842; animation: pulse 1.4s infinite;
t }
t @keyframes pulse { 0%,100% { opacity:1;} 50% { opacity:0.4;} }
t .card {
t   background: rgba(255,255,255,0.06); border-radius: 16px;
t   padding: 24px; margin-bottom: 20px;
t   border-left: 4px solid var(--c1);
t }
t .card h2 {
t   font-size: 1.1em; font-weight: 600; margin-bottom: 16px;
t }
t .badge-tag {
t   font-size: 0.7em; padding: 3px 10px; border-radius: 99px;
t   background: var(--c1); letter-spacing: 0.1em;
t }
t .objetivo {
t   color: #e2e8f0; line-height: 1.6; padding: 14px; 
t   background: rgba(255,255,255,0.04); border-left: 3px solid var(--c2);
t }
t .pasos {
t   color: #cbd5e1; line-height: 1.7; margin-top: 16px;
t   list-style: none; counter-reset: paso;
t }
t .pasos li {
t   counter-increment: paso; padding-left: 36px;
t   position: relative; margin-bottom: 10px;
t }
t .pasos li::before {
t   content: counter(paso); position: absolute; left: 0; top: 0;
t   width: 26px; height: 26px; line-height: 26px;
t   background: var(--c1); border-radius: 50%; text-align: center;
t }
t .pasos strong { color: var(--c2); }
t .consejo {
t   margin-top: 18px; padding: 14px;
t   background: rgba(244,200,66,0.1); border-left: 3px solid #F4C842;
t   color: #fef3c7; font-style: italic;
t }
t .estado-grid {
t   display: grid; grid-template-columns: repeat(3, 1fr); gap: 14px;
t }
t .stat {
t   background: rgba(255,255,255,0.04); padding: 18px;
t   border-radius: 12px; text-align: center;
t }
t .stat .label { font-size: 0.72em; color: #94a3b8; }
t .stat .value { font-size: 2.1em; font-weight: 700; color: var(--c1); }
t .resultado {
t   display: none; text-align: center; padding: 36px;
t   background: rgba(16,185,129,0.18); border-radius: 18px;
t }
t .resultado.fallo { background: rgba(239,68,68,0.18); }
t .resultado h2 { font-size: 1.9em; margin-bottom: 12px; }
t .resultado p { color: #cbd5e1; }
t .resultado .puntos { font-size: 2.6em; font-weight: 700; margin: 18px 0;}
t .volver {
t   display: inline-block; margin-top: 22px; padding: 12px 28px;
t   background: rgba(255,255,255,0.1); color: #fff; border-radius: 9px;
t   text-decoration: none; border: 1px solid rgba(255,255,255,0.2);
t }
t </style>
t </head>
t <body data-modo="1">
t <div class="wrap">
t
t   <section class="hero">
t     <img id="modoIcon" src="icon_mem.png" alt="">
t     <div class="num" id="numModo">MODO -</div>
t     <h1 id="nombreModo">Cargando...</h1>
t     <div class="subtit" id="subtitModo"></div>
t     <div class="jugador">Atleta: <strong id="nombreJugador">-</strong></div>
t     <div class="indicador" id="indicadorEstado">Esperando trama</div>
t   </section>
t
t   <section class="card" id="cardObjetivo">
t     <h2><span class="badge-tag">OBJETIVO</span></h2>
t     <div class="objetivo" id="textoObjetivo"></div>
t   </section>
t
t   <section class="card" id="cardReglas">
t     <h2><span class="badge-tag">INSTRUCCIONES</span></h2>
t     <ol class="pasos" id="reglasLista"></ol>
t     <div class="consejo" id="textoConsejo"></div>
t   </section>
t
t   <section class="card" id="cardEstado">
t     <h2><span class="badge-tag">EN TIEMPO REAL</span></h2>
t     <div class="estado-grid">
t       <div class="stat">
t         <div class="label">Nivel</div>
t         <div class="value" id="statNivel">-</div>
t       </div>
t       <div class="stat">
t         <div class="label" id="statLabelB">Puntos</div>
t         <div class="value" id="statValorB">-</div>
t       </div>
t       <div class="stat">
t         <div class="label" id="statLabelC">Fallos</div>
t         <div class="value" id="statValorC">0</div>
t       </div>
t     </div>
t   </section>
t
t   <section class="resultado" id="cardResultado">
t     <h2 id="tituloResultado"></h2>
t     <p id="subtitResultado"></p>
t     <div class="puntos" id="puntosFinales"></div>
t     <div id="feedbackCognitivo"></div>
t     <a href="panel.cgi" class="volver">Volver al Panel</a>
t   </section>
t
t </div>
t
t <script>
t var MODOS = {};
t
t MODOS[1] = {
t   nombre: 'Memoria de Trabajo',
t   subtit: 'Simon Says cognitivo',
t   icon: 'icon_mem.png',
t   obj: 'Entrenar tu memoria operativa repitiendo secuencias ' +
t        'crecientes de estimulos visuales.',
t   pasos: [
t     'Coloca la mano frente al sensor y mantenla quieta.',
t     'Observa la secuencia en los 4 pads.',
t     'Cuando parpadeen en azul, reproduce la secuencia.',
t     'Tienes 3 segundos entre golpe y golpe.',
t     'Tres niveles consecutivos: 4, 5 y 5 pasos.'
t   ],
t   consejo: 'Verbaliza mentalmente los pads. ' +
t            'La doble codificacion ayuda mucho.',
t   labelB: 'Aciertos',
t   labelC: 'Fallos'
t };
t
t MODOS[2] = {
t   nombre: 'Control de Fuerza',
t   subtit: 'Calibracion psicomotora',
t   icon: 'icon_fza.png',
t   obj: 'Calibrar tu sensibilidad propioceptiva golpeando con ' +
t        'una fuerza concreta.',
t   pasos: [
t     'Coloca la mano frente al sensor para iniciar.',
t     'Un pad parpadeara en azul indicando el objetivo.',
t     'Golpea el pad iluminado con la fuerza objetivo.',
t     'Si tu fuerza queda dentro del margen, el pad es verde.',
t     'La partida son 3 rondas. Busca la maxima precision.'
t   ],
t   consejo: 'Simula el movimiento en el aire para anticipar. ' +
t            'Reduce el error motor.',
t   labelB: 'Aciertos',
t   labelC: 'F.Media'
t };
t
t MODOS[3] = {
t   nombre: 'Inhibicion + Discriminacion',
t   subtit: 'Atencion ejecutiva sostenida',
t   icon: 'icon_inh.png',
t   obj: 'Evaluar tu capacidad para inhibir respuestas a ' +
t        'estimulos distractores.',
t   pasos: [
t     'Cada estimulo es una combinacion de color y sonido.',
t     'Golpea el pad iluminado SOLO si NO es rojo y ES agudo.',
t     'Tres niveles. Los estimulos son cada vez mas rapidos.',
t     'Comision y omision cuentan ambas como fallo.',
t     'Si acumulas 3 fallos en cualquier momento, pierdes.'
t   ],
t   consejo: 'Respira profundo y manten la mano lejos por defecto.',
t   labelB: 'Nivel',
t   labelC: 'Fallos'
t };
t
t MODOS[4] = {
t   nombre: 'Ritmo Constante',
t   subtit: 'Timing motor preciso',
t   icon: 'icon_rit.png',
t   obj: 'Mantener una cadencia motora estable.',
t   pasos: [
t     'Suenan beeps de aviso marcando el periodo.',
t     'Golpea el pad 6 veces seguidas manteniendo el ritmo.',
t     'Cada intervalo debe coincidir con el periodo base.',
t     'Diferentes velocidades por nivel.',
t     'Un solo intervalo fuera de tolerancia y fallas.'
t   ],
t   consejo: 'Cuenta mentalmente entre golpes a ritmo del beep.',
t   labelB: 'Golpes',
t   labelC: 'Fallos'
t };
t
t function feedbackCognitivo(modo, victoria, nivel, errores) {
t   if (modo == 1) return victoria ? 'Excelente memoria.' : 'Sigue.';
t   if (modo == 2) return victoria ? 'Buen control.' : 'Trabaja fuerza.';
t   if (modo == 3) return victoria ? 'Gran inhibicion.' : 'Mas atencion.';
t   if (modo == 4) return victoria ? 'Timing perfecto.' : 'Usa metronomo.';
t   return '';
t }
t
t var params = new URLSearchParams(window.location.search);
t var modoId = parseInt(params.get('modo') || '1');
t var nombre = params.get('nombre') || 'Invitado';
t var cfg = MODOS[modoId] || MODOS[1];
t
t document.body.dataset.modo = modoId;
t document.getElementById('modoIcon').src = cfg.icon;
t document.getElementById('numModo').textContent = 'MODO ' + modoId;
t document.getElementById('nombreModo').textContent = cfg.nombre;
t document.getElementById('subtitModo').textContent = cfg.subtit;
t document.getElementById('nombreJugador').textContent = nombre;
t document.getElementById('textoObjetivo').textContent = cfg.obj;
t document.getElementById('textoConsejo').textContent = cfg.consejo;
t document.getElementById('statLabelB').textContent = cfg.labelB;
t document.getElementById('statLabelC').textContent = cfg.labelC;
t
t var ul = document.getElementById('reglasLista');
t for (var i = 0; i < cfg.pasos.length; i++) {
t   var li = document.createElement('li');
t   li.innerHTML = cfg.pasos[i];
t   ul.appendChild(li);
t }
t
t var partidaTerminada = false;
t
t function mostrarResultado(modo, nivelAlcanzado, codigo) {
t   var r = document.getElementById('cardResultado');
t   document.getElementById('cardEstado').style.display = 'none';
t   document.getElementById('cardReglas').style.display = 'none';
t   document.getElementById('cardObjetivo').style.display = 'none';
t   r.style.display = 'block';
t
t   var v;
t   if (modo == 3) { v = (nivelAlcanzado >= 3 && codigo < 3); }
t   else { v = (codigo == 0); }
t   r.classList.toggle('fallo', !v);
t
t   document.getElementById('tituloResultado').textContent = 
t           v ? '!Victoria!' : 'Partida terminada';
t
t   var p = (modo == 3) ? ((3 - codigo) + ' / 3') : ('Nivel ' + nivelAlcanzado);
t   document.getElementById('puntosFinales').textContent = p;
t   
t   var fb = feedbackCognitivo(modo, v, nivelAlcanzado, codigo);
t   document.getElementById('feedbackCognitivo').textContent = fb;
t   document.getElementById('indicadorEstado').textContent = 'Fin';
t   partidaTerminada = true;
t }
t
t function actualizarEstado(t) {
t   var m = t.match(/Niv\s*(\d+)\s*\|\s*Pts\s*(\d+)/i);
t   if (m) mostrarResultado(modoId, parseInt(m[1]), parseInt(m[2]));
t }
t
t setInterval(function() {
t   if (partidaTerminada) return;
t   fetch('panel.cgi').then(function(r) { return r.text(); })
t   .then(function(html) {
t     var dom = new DOMParser().parseFromString(html, 'text/html');
t     var caja = dom.getElementById('caja_trama');
t     var trama = caja ? caja.textContent.trim() : '';
t     if (trama) actualizarEstado(trama);
t   }).catch(function() {});
t }, 800);
t </script>
t </body>
t </html>
.