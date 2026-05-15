t <!DOCTYPE html>
t <html lang="es">
t <head>
t <meta charset="UTF-8">
t <meta name="viewport" content="width=device-width, initial-scale=1.0">
t <title>R.E.A.C.T. - Panel de Control</title>
t <style>
t * { box-sizing: border-box; margin: 0; padding: 0; }
t body {
t   font-family: -apple-system, "Segoe UI", Roboto, sans-serif;
t   background: url('bg_gear.png') repeat, #f1f5f9;
t   background-size: 512px 512px;
t   min-height: 100vh; color: #0f172a; padding: 24px;
t }
t .topbar {
t   display: flex; justify-content: space-between; align-items: center;
t   max-width: 1100px; margin: 0 auto 28px; padding: 14px 28px;
t   background: #fff; border-radius: 16px;
t   box-shadow: 0 4px 12px rgba(15,23,42,0.06);
t }
t .topbar .brand { display: flex; align-items: center; gap: 14px; }
t .topbar .brand img { height: 40px; }
t .topbar .brand-text {
t   font-weight: 700; color: #1A3942; font-size: 1.15em;
t   letter-spacing: -0.01em;
t }
t .topbar .brand-text small {
t   display: block; font-weight: 500; font-size: 0.72em;
t   color: #64748b; letter-spacing: 0.05em;
t }
t .clock-block { text-align: right; }
t .clock-block .time {
t   font-size: 1.5em; font-weight: 700; color: #0f172a;
t   font-variant-numeric: tabular-nums;
t }
t .clock-block .date {
t   font-size: 0.85em; color: #64748b; margin-top: 2px;
t }
t .container {
t   max-width: 1100px; margin: 0 auto;
t   display: grid; grid-template-columns: 1fr 1fr; gap: 24px;
t }
t .card {
t   background: #fff; border-radius: 16px; padding: 24px;
t   box-shadow: 0 4px 12px rgba(15,23,42,0.05);
t }
t .card.wide { grid-column: 1 / -1; }
t .card h2 {
t   font-size: 1.05em; font-weight: 600; color: #1e293b;
t   margin-bottom: 18px; display: flex; align-items: center; gap: 8px;
t }
t .card h2::before {
t   content: ""; width: 4px; height: 18px;
t   background: #F4C842; border-radius: 2px;
t }
t .status-row {
t   display: flex; align-items: center; gap: 16px; margin-bottom: 14px;
t }
t .status-dot {
t   width: 12px; height: 12px; border-radius: 50%;
t   background: #10b981;
t   box-shadow: 0 0 0 4px rgba(16,185,129,0.2);
t   animation: pulse 1.6s infinite;
t }
t .status-dot.sleep {
t   background: #f59e0b;
t   box-shadow: 0 0 0 4px rgba(245,158,11,0.2);
t }
t .status-dot.error {
t   background: #ef4444;
t   box-shadow: 0 0 0 4px rgba(239,68,68,0.2);
t }
t @keyframes pulse { 0%,100% { opacity: 1;} 50% { opacity: 0.5;} }
t .status-label { font-weight: 600; color: #1e293b; }
t .consumo-bar {
t   width: 100%; height: 14px; background: #e2e8f0;
t   border-radius: 8px; overflow: hidden; margin-top: 8px;
t }
t .consumo-fill {
t   height: 100%; width: 0%; border-radius: 8px;
t   background: linear-gradient(90deg, #10b981 0%, #f59e0b 60%, #ef4444);
t   transition: width 0.5s ease; 
t }
t .consumo-value { font-variant-numeric: tabular-nums;
t                  font-weight: 700; color: #1e293b; }
t .trama {
t   font-family: "JetBrains Mono", "Courier New", monospace;
t   background: #1A3942; color: #4ade80; padding: 14px 16px;
t   border-radius: 10px; font-size: 0.95em; letter-spacing: 0.05em;
t }
t label {
t   display: block; font-weight: 600; color: #334155;
t   font-size: 0.9em; margin-bottom: 6px;
t }
t input[type=text] {
t   width: 100%; padding: 11px 14px; border: 1px solid #cbd5e1;
t   border-radius: 8px; font-size: 0.95em; font-family: inherit;
t   background: #fff; color: #0f172a; outline: none;
t   transition: border 0.2s, box-shadow 0.2s;
t }
t input[type=text]:focus {
t   border-color: #F4C842; box-shadow: 0 0 0 3px rgba(244,200,66,0.2);
t }
t .form-row { margin-bottom: 16px; }
t .btn {
t   display: inline-block; padding: 13px 22px; border: none;
t   border-radius: 9px; font-weight: 600; font-size: 0.95em;
t   cursor: pointer; transition: all 0.2s ease;
t   font-family: inherit;
t }
t .btn-primary {
t   background: linear-gradient(135deg, #F4C842 0%, #d97706 100%);
t   color: #1A3942; box-shadow: 0 4px 12px rgba(244,200,66,0.35);
t }
t .btn-primary:hover {
t   transform: translateY(-1px);
t   box-shadow: 0 6px 18px rgba(244,200,66,0.5);
t }
t .btn-secondary { background: #1A3942; color: #fff; }
t .btn-secondary:hover { background: #0f172a; }
t .btn-warning {
t   background: #f59e0b; color: #fff;
t   box-shadow: 0 4px 12px rgba(245,158,11,0.3);
t }
t .btn-warning:hover { background: #d97706; transform: translateY(-1px); }
t .btn-block { width: 100%; }
t .modo-grid {
t   display: grid; grid-template-columns: 1fr 1fr; gap: 12px;
t   margin-top: 10px;
t }
t .modo-card {
t   padding: 14px; border: 2px solid #e2e8f0;
t   border-radius: 12px; cursor: pointer;
t   transition: all 0.2s; background: #f8fafc;
t   display: flex; gap: 12px; align-items: flex-start;
t }
t .modo-card img.icono {
t   width: 52px; height: 52px; flex-shrink: 0;
t   filter: drop-shadow(0 2px 4px rgba(0,0,0,0.1));
t }
t .modo-card .info { flex: 1; min-width: 0; }
t .modo-card .num {
t   font-weight: 700; font-size: 0.75em; letter-spacing: 0.12em;
t }
t .modo-card .name { font-weight: 600; margin-top: 2px; color: #0f172a; }
t .modo-card .desc {
t   font-size: 0.78em; color: #64748b; margin-top: 4px; line-height: 1.4;
t }
t .modo-card.m1 { background: linear-gradient(135deg, #fff, #EEF2FF); }
t .modo-card.m1 .num { color: #4F46E5; }
t .modo-card.m1:hover, .modo-card.m1.selected { 
t   border-color: #4F46E5; background: #E0E7FF; 
t }
t .modo-card.m2 { background: linear-gradient(135deg, #fff, #FFFBEB); }
t .modo-card.m2 .num { color: #F59E0B; }
t .modo-card.m2:hover, .modo-card.m2.selected { 
t   border-color: #F59E0B; background: #FEF3C7; 
t }
t .modo-card.m3 { background: linear-gradient(135deg, #fff, #FDF2F8); }
t .modo-card.m3 .num { color: #DB2777; }
t .modo-card.m3:hover, .modo-card.m3.selected { 
t   border-color: #DB2777; background: #FCE7F3; 
t }
t .modo-card.m4 { background: linear-gradient(135deg, #fff, #F0FDFA); }
t .modo-card.m4 .num { color: #0D9488; }
t .modo-card.m4:hover, .modo-card.m4.selected { 
t   border-color: #0D9488; background: #CCFBF1; 
t }
t .actions-grid {
t   display: grid; grid-template-columns: 1fr 1fr; gap: 12px;
t }
t @media (max-width: 800px) {
t   .container { grid-template-columns: 1fr; }
t   .topbar { flex-direction: column; gap: 12px; align-items: flex-start; }
t   .clock-block { text-align: left; }
t   .modo-grid, .actions-grid { grid-template-columns: 1fr; }
t }
t </style>
t </head>
t <body>
t
t <header class="topbar">
t   <div class="brand">
t     <img src="logo.png" alt="REACT">
t     <div class="brand-text">
t       R.E.A.C.T.
t       <small>Panel de Control &middot; Nodo A</small>
t     </div>
t   </div>
t   <div class="clock-block">
t     <div class="time" id="reloj">
c h 1
t     </div>
t     <div class="date" id="fecha">
c h 2
t     </div>
t   </div>
t </header>
t
t <div class="container">
t
t   <section class="card">
t     <h2>Estado del Nodo B</h2>
t     <div class="status-row">
t       <div class="status-dot" id="statusDot"></div>
t       <div class="status-label" id="statusLabel">
c r 1
t       </div>
t     </div>
t     <label>Consumo en tiempo real</label>
t     <div class="consumo-bar">
t       <div id="consumoFill" class="consumo-fill"></div>
t     </div>
t     <div style="display:flex;justify-content:space-between;
t                 margin-top:6px;font-size:0.85em;color:#64748b;">
t       <span>0 mA</span>
t       <span class="consumo-value">
t         <span id="consumoVal">
c r 3
t         </span> mA
t       </span>
t       <span>100 mA</span>
t     </div>
t   </section>
t
t   <section class="card">
t     <h2>Ultima Trama Recibida</h2>
t     <div class="trama" id="caja_trama">
c r 2
t     </div>
t     <div style="margin-top:14px;font-size:0.82em;color:#64748b;">
t       Formato: <code>SOF CMD P0 P1 P2 CHK</code>
t     </div>
t   </section>
t
t   <section class="card wide">
t     <h2>Iniciar Entrenamiento Cognitivo</h2>
t     <form id="formPartida" onsubmit="return iniciarPartida(event)">
t       <div class="form-row">
t         <label>Nombre del atleta</label>
t         <input type="text" id="nombreJugador" name="jugador"
t                maxlength="15" value="Invitado" required>
t       </div>
t       <div class="form-row">
t         <label>Modo de juego</label>
t         <input type="hidden" id="modoSeleccionado" name="modo" value="1">
t         <div class="modo-grid">
t
t           <div class="modo-card m1 selected" data-modo="1" 
t                onclick="elegirModo(this)">
t             <img src="icon_mem.png" alt="" class="icono">
t             <div class="info">
t               <div class="num">MODO 1</div>
t               <div class="name">Memoria de Trabajo</div>
t               <div class="desc">Simon Says de pads.</div>
t             </div>
t           </div>
t
t           <div class="modo-card m2" data-modo="2" 
t                onclick="elegirModo(this)">
t             <img src="icon_fza.png" alt="" class="icono">
t             <div class="info">
t               <div class="num">MODO 2</div>
t               <div class="name">Control de Fuerza</div>
t               <div class="desc">Golpea con fuerza exacta.</div>
t             </div>
t           </div>
t
t           <div class="modo-card m3" data-modo="3" 
t                onclick="elegirModo(this)">
t             <img src="icon_inh.png" alt="" class="icono">
t             <div class="info">
t               <div class="num">MODO 3</div>
t               <div class="name">Inhibicion</div>
t               <div class="desc">Pulsa solo si NO rojo.</div>
t             </div>
t           </div>
t
t           <div class="modo-card m4" data-modo="4" 
t                onclick="elegirModo(this)">
t             <img src="icon_rit.png" alt="" class="icono">
t             <div class="info">
t               <div class="num">MODO 4</div>
t               <div class="name">Ritmo Constante</div>
t               <div class="desc">Mantiene la cadencia.</div>
t             </div>
t           </div>
t         </div>
t       </div>
t       <button type="submit" class="btn btn-primary btn-block">
t         INICIAR PARTIDA
t       </button>
t     </form>
t   </section>
t
t   <section class="card wide">
t     <h2>Acciones Rapidas</h2>
t     <div class="actions-grid">
t       <button class="btn btn-secondary" 
t               onclick="window.location.href='records.cgi'">
t         Ver Records (EEPROM)
t       </button>
t       <button class="btn btn-warning" onclick="confirmarSleep()">
t         Forzar Bajo Consumo (Sleep)
t       </button>
t     </div>
t     <div style="margin-top:14px;font-size:0.82em; 
t                 color:#64748b;text-align:center;">
t       Tambien disponible: 
t       <a href="time.cgi" style="color:#0D9488;text-decoration:none;">
t         Config RTC
t       </a>
t     </div>
t   </section>
t </div>
t
t <div id="webCommand" style="display:none;">
c r 4
t </div>
t
t <script>
t   function elegirModo(card) {
t     var cards = document.querySelectorAll('.modo-card');
t     for (var i = 0; i < cards.length; i++) {
t       cards[i].classList.remove('selected');
t     }
t     card.classList.add('selected');
t     document.getElementById('modoSeleccionado').value = card.dataset.modo;
t   }
t
t   function iniciarPartida(e) {
t     e.preventDefault();
t     var nombre = document.getElementById('nombreJugador').value;
t     if (!nombre.trim()) nombre = 'Invitado';
t     var modo = document.getElementById('modoSeleccionado').value;
t     var data = new URLSearchParams();
t     data.append('jugador', nombre);
t     data.append('modo', modo);
t     fetch('panel.cgi', { method: 'POST', body: data });
t     setTimeout(function() {
t       var url = 'juego.cgi?modo=' + modo + '&nombre=' 
t               + encodeURIComponent(nombre);
t       window.location.href = url;
t     }, 200);
t     return false;
t   }
t
t   function confirmarSleep() {
t     var msg = 'El Nodo B entrara en bajo consumo. ';
t     msg += 'Solo podra ser despertado despues.\n\n Continuar?';
t     if (!confirm(msg)) return;
t     var data = new URLSearchParams();
t     data.append('ctrl', 'sleep');
t     fetch('panel.cgi', { method: 'POST', body: data });
t   }
t
t   function actualizarBarraConsumo(mA) {
t     var valor = parseInt(mA) || 0;
t     if (valor < 0) valor = 0;
t     if (valor > 100) valor = 100;
t     document.getElementById('consumoFill').style.width = valor + '%';
t     document.getElementById('consumoVal').textContent = valor;
t   }
t
t   function ajustarDot(texto) {
t     var dot = document.getElementById('statusDot');
t     dot.classList.remove('sleep', 'error');
t     if (/sleep/i.test(texto)) { dot.classList.add('sleep'); }
t     else if (/caido/i.test(texto)) { dot.classList.add('error'); }
t   }
t
t   setInterval(function() {
t     fetch('panel.cgi').then(function(r) { return r.text(); })
t     .then(function(html) {
t       var dom = new DOMParser().parseFromString(html, 'text/html');
t       function get(id) { 
t         var el = dom.getElementById(id); return el ? el.innerHTML : ''; 
t       }
t       document.getElementById('reloj').innerHTML = get('reloj');
t       document.getElementById('fecha').innerHTML = get('fecha');
t       document.getElementById('statusLabel').innerHTML = get('statusLabel');
t       document.getElementById('caja_trama').innerHTML = get('caja_trama');
t       
t       var elMA = dom.getElementById('consumoVal');
t       var mA = elMA ? elMA.textContent : '0';
t       actualizarBarraConsumo(mA);
t       ajustarDot(document.getElementById('statusLabel').textContent);
t       
t       var cmdEl = dom.getElementById('webCommand');
t       var cmd = cmdEl ? cmdEl.textContent.trim() : '';
t       if (cmd.indexOf('NAV:') === 0) {
t         var m_juego = cmd.split(':')[1];
t         var inp = document.getElementById('nombreJugador');
t         var nj = (inp && inp.value.trim()) ? inp.value.trim() : 'Invitado';
t         var dir = 'juego.cgi?modo=' + m_juego + '&nombre=' 
t                 + encodeURIComponent(nj);
t         window.location.href = dir;
t       }
t     }).catch(function() {});
t   }, 1000);
t </script>
t </body>
t </html>
.