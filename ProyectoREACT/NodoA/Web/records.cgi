t <!DOCTYPE html>
t <html lang="es">
t <head>
t <meta charset="UTF-8">
t <meta name="viewport" content="width=device-width, initial-scale=1.0">
t <title>R.E.A.C.T. - Salon de la Fama</title>
t <style>
t * { box-sizing: border-box; margin: 0; padding: 0; }
t body {
t   font-family: -apple-system, "Segoe UI", Roboto, sans-serif;
t   background: linear-gradient(135deg, #0f172a 0%, #1A3942 45%, #1e3a8a 100%);
t   min-height: 100vh; color: #f8fafc; padding: 24px;
t }
t .wrap { max-width: 900px; margin: 0 auto; }
t .topbar { display: flex; justify-content: space-between; align-items: center; margin-bottom: 30px; }
t .logo { height: 50px; filter: drop-shadow(0 4px 10px rgba(244,200,66,0.2)); }
t .btn-back { color: #F4C842; text-decoration: none; font-weight: 600; font-size: 0.9em; }
t .card {
t   background: rgba(255,255,255,0.05); border-radius: 20px; padding: 32px;
t   border: 1px solid rgba(255,255,255,0.1); box-shadow: 0 10px 40px rgba(0,0,0,0.3);
t }
t h2 { margin-bottom: 24px; color: #F4C842; display: flex; align-items: center; gap: 10px; }
t table { width: 100%; border-collapse: collapse; margin-top: 10px; font-size: 0.95em; }
t th { text-align: left; padding: 14px; color: #94a3b8; border-bottom: 2px solid rgba(255,255,255,0.1); }
t td { padding: 14px; border-bottom: 1px solid rgba(255,255,255,0.05); color: #cbd5e1; }
t tr:hover { background: rgba(255,255,255,0.02); }
t .pts { color: #F4C842; font-weight: 700; }
t .btn-wipe {
t   margin-top: 30px; padding: 12px 24px; background: #ef4444; color: white;
t   border: none; border-radius: 8px; cursor: pointer; font-weight: 600; transition: 0.2s;
t }
t .btn-wipe:hover { background: #dc2626; transform: translateY(-1px); }
t </style>
t </head>
t <body>
t <div class="wrap">
t   <header class="topbar">
t     <img src="logo.png" alt="REACT" class="logo">
t     <a href="panel.cgi" class="btn-back"> VOLVER AL PANEL</a>
t   </header>
t   <div class="card">
t     <h2> Salon de la Fama (EEPROM)</h2>
t     <table>
t       <thead><tr><th>ATLETA</th><th>NIVEL</th><th>PUNTOS</th><th>FECHA Y HORA</th></tr></thead>
t       <tbody>
t         <tr><td>
c k 0 1
t         </td><td>
c k 0 2
t         </td><td class="pts">
c k 0 3
t         </td><td>
c k 0 4
t         </td></tr>
t         <tr><td>
c k 1 1
t         </td><td>
c k 1 2
t         </td><td class="pts">
c k 1 3
t         </td><td>
c k 1 4
t         </td></tr>
t         <tr><td>
c k 2 1
t         </td><td>
c k 2 2
t         </td><td class="pts">
c k 2 3
t         </td><td>
c k 2 4
t         </td></tr>
t         <tr><td>
c k 3 1
t         </td><td>
c k 3 2
t         </td><td class="pts">
c k 3 3
t         </td><td>
c k 3 4
t         </td></tr>
t         <tr><td>
c k 4 1
t         </td><td>
c k 4 2
t         </td><td class="pts">
c k 4 3
t         </td><td>
c k 4 4
t         </td></tr>
t         <tr><td>
c k 5 1
t         </td><td>
c k 5 2
t         </td><td class="pts">
c k 5 3
t         </td><td>
c k 5 4
t         </td></tr>
t         <tr><td>
c k 6 1
t         </td><td>
c k 6 2
t         </td><td class="pts">
c k 6 3
t         </td><td>
c k 6 4
t         </td></tr>
t         <tr><td>
c k 7 1
t         </td><td>
c k 7 2
t         </td><td class="pts">
c k 7 3
t         </td><td>
c k 7 4
t         </td></tr>
t         <tr><td>
c k 8 1
t         </td><td>
c k 8 2
t         </td><td class="pts">
c k 8 3
t         </td><td>
c k 8 4
t         </td></tr>
t         <tr><td>
c k 9 1
t         </td><td>
c k 9 2
t         </td><td class="pts">
c k 9 3
t         </td><td>
c k 9 4
t         </td></tr>
t       </tbody>
t     </table>
t     <button class="btn-wipe" onclick="borrar()">BORRAR TODO EL HISTORIAL</button>
t   </div>
t </div>
t <script>
t   function borrar() {
t     if(!confirm("¿Seguro que quieres borrar la EEPROM?")) return;
t     fetch('records.cgi', {method: 'POST', body: new URLSearchParams({'ctrl':'wipe_records'})})
t     .then(() => location.reload());
t   }
t </script>
t </body>
t </html>
.