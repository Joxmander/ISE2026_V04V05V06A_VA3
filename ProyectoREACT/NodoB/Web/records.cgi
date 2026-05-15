t <!DOCTYPE html>
t <html lang="es">
t <head>
t <meta charset="UTF-8">
t <meta http-equiv="refresh" content="10">
t <meta name="viewport" content="width=device-width, initial-scale=1.0">
t <title>R.E.A.C.T. - Top 10 (EEPROM)</title>
t <style>
t * { box-sizing: border-box; margin: 0; padding: 0; }
t body { font-family: -apple-system, sans-serif;
t        background: linear-gradient(180deg, #f1f5f9 0%, #e2e8f0 100%);
t        min-height: 100vh; color: #0f172a; padding: 24px; }
t .wrap { max-width: 900px; margin: 0 auto; }
t .topbar { display: flex; justify-content: space-between; align-items: center;
t           margin-bottom: 24px; padding: 14px 24px; background: #fff;
t           border-radius: 14px; box-shadow: 0 4px 12px rgba(15,23,42,0.06); }
t .topbar h1 { font-size: 1.25em; color: #1e3a8a; display: flex;
t              align-items: center; gap: 12px; }
t .topbar h1 img { height: 32px; }
t .topbar a { color: #2563eb; text-decoration: none; font-weight: 600; }
t .card { background: #fff; border-radius: 16px; padding: 28px;
t         box-shadow: 0 4px 12px rgba(15,23,42,0.05); }
t .card h2 { font-size: 1.15em; font-weight: 600; color: #1e293b;
t            margin-bottom: 18px; display: flex; align-items: center; gap: 8px; }
t .card h2::before { content: ""; width: 4px; height: 20px;
t                    background: #2563eb; border-radius: 2px; }
t table { width: 100%; border-collapse: collapse; margin-top: 8px; }
t th { text-align: left; padding: 12px 14px; font-size: 0.78em;
t      color: #64748b; text-transform: uppercase; letter-spacing: 0.08em;
t      border-bottom: 2px solid #e2e8f0; font-weight: 600; }
t td { padding: 14px; border-bottom: 1px solid #f1f5f9; font-size: 0.95em; }
t tr:hover td { background: #f8fafc; }
t .puntuacion { font-weight: 700; color: #2563eb; }
t .nombre { font-weight: 600; }
t .fechita { color: #64748b; font-size: 0.88em; }
t .vacio { text-align: center; padding: 40px 20px; color: #64748b; }
t .actions { display: flex; gap: 12px; margin-top: 20px; }
t .btn { padding: 10px 18px; border-radius: 9px; border: none;
t        font-weight: 600; font-size: 0.9em; cursor: pointer; }
t .btn-primary { background: linear-gradient(135deg, #06b6d4, #2563eb);
t                color: #fff; }
t .btn-ghost { background: #f8fafc; color: #1e293b; border: 1px solid #e2e8f0; }
t @media (max-width: 700px) { th.col-fecha, td.col-fecha { display: none; } }
t </style>
t </head>
t <body>
t <div class="wrap">
t   <header class="topbar">
t     <h1><img src="logo.png" alt="R">Top 10 Records (EEPROM)</h1>
t     <a href="panel.cgi">&larr; Volver al Panel</a>
t   </header>
t   <div class="card">
t     <h2>Salon de la Fama</h2>
t     <table>
t       <thead>
t         <tr>
t           <th>Atleta</th><th>Nivel</th><th>Puntos</th>
t           <th class="col-fecha">Fecha &middot; Hora</th>
t         </tr>
t       </thead>
t       <tbody id="tablaRecords">
t         <tr>
t           <td><span class="nombre" id="celNombre">
c k 0 1
t           </span></td>
t           <td>
c k 0 2
t           </td>
t           <td><span class="puntuacion">
c k 0 3
t           </span></td>
t           <td class="col-fecha"><span class="fechita">
c k 0 4
t           </span></td>
t         </tr>
t         <tr>
t           <td><span class="nombre">
c k 1 1
t           </span></td>
t           <td>
c k 1 2
t           </td>
t           <td><span class="puntuacion">
c k 1 3
t           </span></td>
t           <td class="col-fecha"><span class="fechita">
c k 1 4
t           </span></td>
t         </tr>
t         <tr>
t           <td><span class="nombre">
c k 2 1
t           </span></td>
t           <td>
c k 2 2
t           </td>
t           <td><span class="puntuacion">
c k 2 3
t           </span></td>
t           <td class="col-fecha"><span class="fechita">
c k 2 4
t           </span></td>
t         </tr>
t         <tr>
t           <td><span class="nombre">
c k 3 1
t           </span></td>
t           <td>
c k 3 2
t           </td>
t           <td><span class="puntuacion">
c k 3 3
t           </span></td>
t           <td class="col-fecha"><span class="fechita">
c k 3 4
t           </span></td>
t         </tr>
t         <tr>
t           <td><span class="nombre">
c k 4 1
t           </span></td>
t           <td>
c k 4 2
t           </td>
t           <td><span class="puntuacion">
c k 4 3
t           </span></td>
t           <td class="col-fecha"><span class="fechita">
c k 4 4
t           </span></td>
t         </tr>
t         <tr>
t           <td><span class="nombre">
c k 5 1
t           </span></td>
t           <td>
c k 5 2
t           </td>
t           <td><span class="puntuacion">
c k 5 3
t           </span></td>
t           <td class="col-fecha"><span class="fechita">
c k 5 4
t           </span></td>
t         </tr>
t         <tr>
t           <td><span class="nombre">
c k 6 1
t           </span></td>
t           <td>
c k 6 2
t           </td>
t           <td><span class="puntuacion">
c k 6 3
t           </span></td>
t           <td class="col-fecha"><span class="fechita">
c k 6 4
t           </span></td>
t         </tr>
t         <tr>
t           <td><span class="nombre">
c k 7 1
t           </span></td>
t           <td>
c k 7 2
t           </td>
t           <td><span class="puntuacion">
c k 7 3
t           </span></td>
t           <td class="col-fecha"><span class="fechita">
c k 7 4
t           </span></td>
t         </tr>
t         <tr>
t           <td><span class="nombre">
c k 8 1
t           </span></td>
t           <td>
c k 8 2
t           </td>
t           <td><span class="puntuacion">
c k 8 3
t           </span></td>
t           <td class="col-fecha"><span class="fechita">
c k 8 4
t           </span></td>
t         </tr>
t         <tr>
t           <td><span class="nombre">
c k 9 1
t           </span></td>
t           <td>
c k 9 2
t           </td>
t           <td><span class="puntuacion">
c k 9 3
t           </span></td>
t           <td class="col-fecha"><span class="fechita">
c k 9 4
t           </span></td>
t         </tr>
t       </tbody>
t     </table>
t     <div class="actions">
t       <button class="btn btn-primary" onclick="window.location.reload()">Refrescar</button>
t       <button class="btn btn-ghost" onclick="borrarRecords()">Borrar Todos los Records</button>
t     </div>
t     <div style="margin-top:18px; font-size:0.85em; color:#64748b;">
t       Auto-refresco activado (cada 10 seg). El Top 10 se ordena por puntos.
t     </div>
t   </div>
t </div>
t <script>
t   function borrarRecords() {
t     if(confirm('Borrar todos los records?')) {
t       const data = new URLSearchParams();
t       data.append('ctrl', 'wipe_records');
t       fetch('records.cgi', { method: 'POST', body: data })
t         .then(() => { setTimeout(() => window.location.reload(), 400); });
t     }
t   }
t   window.addEventListener('DOMContentLoaded', () => {
t     const n0 = document.getElementById('celNombre').textContent.trim();
t     if (!n0 || n0 === '--') {
t       document.getElementById('tablaRecords').innerHTML =
t         '<tr><td colspan="4" class="vacio">' +
t         '<strong>No hay records</strong>Juega una partida.</td></tr>';
t     }
t   });
t </script>
t </body>
t </html>
.