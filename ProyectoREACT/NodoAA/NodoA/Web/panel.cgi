t <!DOCTYPE html>
t <html lang="es">
t <head>
t <meta charset="UTF-8">
t <meta name="viewport" content="width=device-width, initial-scale=1.0">
t <title>R.E.A.C.T. - Panel de Control</title>
t <style>
t * { box-sizing: border-box; margin: 0; padding: 0; }
t body { font-family: -apple-system, "Segoe UI", Roboto, sans-serif;
t        background: linear-gradient(180deg, #f1f5f9 0%, #e2e8f0 100%);
t        min-height: 100vh; color: #0f172a; padding: 24px; }
t .topbar { display: flex; justify-content: space-between; align-items: center;
t           max-width: 1100px; margin: 0 auto 28px; padding: 14px 28px;
t           background: #fff; border-radius: 16px;
t           box-shadow: 0 4px 12px rgba(15,23,42,0.06); }
t .topbar .brand { display: flex; align-items: center; gap: 14px; }
t .topbar .brand img { height: 38px; }
t .topbar .brand-text { font-weight: 700; color: #1e3a8a; font-size: 1.1em;
t                       letter-spacing: -0.01em; }
t .clock-block { text-align: right; }
t .clock-block .time { font-size: 1.4em; font-weight: 700; color: #0f172a;
t                      font-variant-numeric: tabular-nums; }
t .clock-block .date { font-size: 0.85em; color: #64748b; margin-top: 2px; }
t .container { max-width: 1100px; margin: 0 auto;
t              display: grid; grid-template-columns: 1fr 1fr; gap: 24px; }
t .card { background: #fff; border-radius: 16px; padding: 24px;
t         box-shadow: 0 4px 12px rgba(15,23,42,0.05); }
t .card.wide { grid-column: 1 / -1; }
t .card h2 { font-size: 1.05em; font-weight: 600; color: #1e293b;
t            margin-bottom: 18px; display: flex; align-items: center; gap: 8px; }
t .card h2::before { content: ""; width: 4px; height: 18px;
t                    background: #2563eb; border-radius: 2px; }
t .status-row { display: flex; align-items: center; gap: 16px; margin-bottom: 14px; }
t .status-dot { width: 12px; height: 12px; border-radius: 50%;
t               background: #10b981; box-shadow: 0 0 0 4px rgba(16,185,129,0.2);
t               animation: pulse 1.6s infinite; }
t .status-dot.sleep { background: #f59e0b; box-shadow: 0 0 0 4px rgba(245,158,11,0.2); }
t .status-dot.error { background: #ef4444; box-shadow: 0 0 0 4px rgba(239,68,68,0.2); }
t @keyframes pulse { 0%,100% { opacity: 1; } 50% { opacity: 0.5; } }
t .status-label { font-weight: 600; color: #1e293b; }
t .consumo-bar { width: 100%; height: 14px; background: #e2e8f0;
t                border-radius: 8px; overflow: hidden; margin-top: 8px; }
t .consumo-fill { height: 100%;
t                 background: linear-gradient(90deg, #10b981 0%, #f59e0b 60%, #ef4444 100%);
t                 width: 0%; transition: width 0.8s ease; border-radius: 8px; }
t .consumo-value { font-variant-numeric: tabular-nums;
t                  font-weight: 700; color: #1e293b; }
t .trama { font-family: "JetBrains Mono", "Courier New", monospace;
t          background: #0f172a; color: #4ade80; padding: 14px 16px;
t          border-radius: 10px; font-size: 0.95em; letter-spacing: 0.05em; }
t label { display: block; font-weight: 600; color: #334155;
t         font-size: 0.9em; margin-bottom: 6px; }
t input[type=text], select {
t   width: 100%; padding: 11px 14px; border: 1px solid #cbd5e1;
t   border-radius: 8px; font-size: 0.95em; font-family: inherit;
t   background: #fff; color: #0f172a; outline: none;
t   transition: border 0.2s, box-shadow 0.2s; }
t input[type=text]:focus, select:focus { border-color: #2563eb;
t   box-shadow: 0 0 0 3px rgba(37,99,235,0.15); }
t .form-row { margin-bottom: 16px; }
t .btn { display: inline-block; padding: 12px 22px; border: none;
t        border-radius: 9px; font-weight: 600; font-size: 0.95em;
t        cursor: pointer; transition: all 0.2s ease;
t        font-family: inherit; text-align: center; }
t .btn-primary { background: linear-gradient(135deg, #06b6d4 0%, #2563eb 100%);
t                color: #fff; box-shadow: 0 4px 12px rgba(37,99,235,0.3); }
t .btn-primary:hover { transform: translateY(-1px);
t                      box-shadow: 0 6px 16px rgba(37,99,235,0.4); }
t .btn-secondary { background: #1e293b; color: #fff; }
t .btn-secondary:hover { background: #334155; }
t .btn-warning { background: #f59e0b; color: #fff;
t                box-shadow: 0 4px 12px rgba(245,158,11,0.3); }
t .btn-warning:hover { background: #d97706; transform: translateY(-1px); }
t .btn-block { width: 100%; }
t .modo-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px;
t              margin-top: 10px; }
t .modo-card { padding: 12px; border: 2px solid #e2e8f0; border-radius: 10px;
t              cursor: pointer; transition: all 0.2s; background: #f8fafc; }
t .modo-card:hover { border-color: #2563eb; background: #eff6ff; }
t .modo-card.selected { border-color: #2563eb; background: #dbeafe; }
t .modo-card .num { font-weight: 700; color: #2563eb; font-size: 0.85em;
t                   letter-spacing: 0.1em; }
t .modo-card .name { font-weight: 600; margin-top: 2px; color: #0f172a; }
t .modo-card .desc { font-size: 0.78em; color: #64748b; margin-top: 4px;
t                    line-height: 1.4; }
t .actions-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
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
t     <div class="brand-text">R.E.A.C.T. &middot; Panel de Control</div>
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
t     <div class="consumo-bar"><div class="consumo-fill" id="consumoFill"></div></div>
t     <div style="display:flex;justify-content:space-between;margin-top:6px;
t                 font-size:0.85em;color:#64748b;">
t       <span>0 mA</span>
t       <span class="consumo-value"><span id="consumoVal">
c r 3
t       </span> mA</span>
t       <span>100 mA</span>
t     </div>
t   </section>
t
t   <section class="card">
t     <h2>Ultima Trama Recibida</h2>
t     <div class="trama" id="caja_trama">
c r 2
t     </div>
t     <div style="margin-top: 14px; font-size: 0.82em; color: #64748b;">
t       Formato: <code>SOF CMD P0 P1 P2 CHK</code>
t     </div>
t   </section>
t
t   <section class="card wide">
t     <h2>Iniciar Entrenamiento Cognitivo</h2>
t     <form id="formPartida" onsubmit="return irAInstrucciones(event)">
t       <div class="form-row">
t         <label>Nombre del atleta</label>
t         <input type="text" id="nombreJugador" name="jugador" maxlength="15"
t                value="Invitado" required>
t       </div>
t       <div class="form-row">
t         <label>Modo de juego</label>
t         <input type="hidden" id="modoSeleccionado" name="modo" value="1">
t         <div class="modo-grid">
t           <div class="modo-card selected" data-modo="1" onclick="elegirModo(this)">
t             <div class="num">MODO 1</div>
t             <div class="name">Memoria de Trabajo</div>
t             <div class="desc">Simon Says: repite la secuencia.</div>
t           </div>
t           <div class="modo-card" data-modo="2" onclick="elegirModo(this)">
t             <div class="num">MODO 2</div>
t             <div class="name">Control de Fuerza</div>
t             <div class="desc">Golpea con la fuerza objetivo (1-100).</div>
t           </div>
t           <div class="modo-card" data-modo="3" onclick="elegirModo(this)">
t             <div class="num">MODO 3</div>
t             <div class="name">Inhibicion + Discriminacion</div>
t             <div class="desc">Pulsa solo si NO rojo + agudo.</div>
t           </div>
t           <div class="modo-card" data-modo="4" onclick="elegirModo(this)">
t             <div class="num">MODO 4</div>
t             <div class="name">Ritmo Constante</div>
t             <div class="desc">Mantiene cadencia constante.</div>
t           </div>
t         </div>
t       </div>
t       <button type="submit" class="btn btn-primary btn-block">SIGUIENTE (Instrucciones)</button>
t     </form>
t   </section>
t
t   <section class="card wide">
t     <h2>Acciones Rapidas</h2>
t     <div class="actions-grid">
t       <div class="btn btn-secondary" onclick="window.location.href='records.cgi'">
t         Ver Records (EEPROM)
t       </div>
t       <button class="btn btn-warning" onclick="confirmarSleep()">
t         Forzar Bajo Consumo (Sleep)
t       </button>
t     </div>
t     <div style="margin-top: 14px; font-size: 0.82em; color: #64748b; text-align: center;">
t       Tambien disponible: <a href="time.cgi" style="color:#2563eb;">Config. NTP</a>
t     </div>
t   </section>
t
t </div>
t
t <script>
t   function elegirModo(card) {
t     document.querySelectorAll('.modo-card').forEach(c => c.classList.remove('selected'));
t     card.classList.add('selected');
t     document.getElementById('modoSeleccionado').value = card.dataset.modo;
t   }
t
t   function irAInstrucciones(e) {
t     e.preventDefault();
t     const nombre = document.getElementById('nombreJugador').value.trim() || 'Invitado';
t     const modo   = document.getElementById('modoSeleccionado').value;
t     
t     // Navegamos a la pagina de instrucciones SIN enviar comando a la placa aun.
t     let url = 'juego.cgi?modo=' + modo + '&nombre=' + encodeURIComponent(nombre);
t     window.location.href = url;
t     return false;
t   }
t
t   function confirmarSleep() {
t     let msg = 'El Nodo B entrara en modo sleep.\\n\\nContinuar?';
t     if (!confirm(msg)) return;
t     const data = new URLSearchParams();
t     data.append('ctrl', 'sleep');
t     fetch('panel.cgi', { method:'POST', body: data });
t   }
t
t   function actualizarBarraConsumo(mA) {
t     const valor = Math.max(0, Math.min(100, parseInt(mA) || 0));
t     document.getElementById('consumoFill').style.width = valor + '%';
t     document.getElementById('consumoVal').textContent = valor;
t   }
t
t   function ajustarDot(texto) {
t     const dot = document.getElementById('statusDot');
t     dot.classList.remove('sleep', 'error');
t     if (/sleep/i.test(texto)) dot.classList.add('sleep');
t     else if (/desconectado|caido/i.test(texto)) dot.classList.add('error');
t   }
t
t   actualizarBarraConsumo(document.getElementById('consumoVal').textContent);
t   ajustarDot(document.getElementById('statusLabel').textContent);
t
t   setInterval(() => {
t     fetch('panel.cgi').then(r => r.text()).then(html => {
t       const dom = new DOMParser().parseFromString(html, 'text/html');
t       const get = id => dom.getElementById(id)?.innerHTML || '';
t       document.getElementById('reloj').innerHTML        = get('reloj');
t       document.getElementById('fecha').innerHTML        = get('fecha');
t       document.getElementById('statusLabel').innerHTML  = get('statusLabel');
t       document.getElementById('caja_trama').innerHTML   = get('caja_trama');
t       const mA = dom.getElementById('consumoVal')?.textContent || '0';
t       actualizarBarraConsumo(mA);
t       ajustarDot(document.getElementById('statusLabel').textContent);
t     }).catch(() => {});
t   }, 1000);
t </script>
t </body>
t </html>
.