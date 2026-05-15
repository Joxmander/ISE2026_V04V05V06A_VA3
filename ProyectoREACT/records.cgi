t <!DOCTYPE html>
t <html lang="es">
t <head>
t <meta charset="UTF-8">
t <meta http-equiv="refresh" content="10">
t <meta name="viewport" content="width=device-width, initial-scale=1.0">
t <title>Top 10 Records</title>
t <style>
t * { box-sizing: border-box; margin: 0; padding: 0; }
t body { font-family: sans-serif; padding: 24px; }
t .wrap { max-width: 900px; margin: 0 auto; }
t .card { background: #fff; padding: 28px; border-radius: 16px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); }
t table { width: 100%; border-collapse: collapse; margin-top: 10px; }
t th, td { padding: 10px; border-bottom: 1px solid #ddd; text-align: left; }
t .btn { padding: 10px; cursor: pointer; margin-top: 10px;}
t </style>
t </head>
t <body>
t <div class="wrap">
t   <div class="card">
t     <h2>Salon de la Fama (EEPROM)</h2>
t     <a href="panel.cgi">Volver al Panel</a>
t     <table>
t       <thead>
t         <tr><th>Atleta</th><th>Nivel</th><th>Puntos</th><th>Fecha</th></tr>
t       </thead>
t       <tbody id="tablaRecords">
t         <tr>
t           <td><span id="celNombre">
c k 0 1
t           </span></td>
t           <td>
c k 0 2
t           </td>
t           <td><span id="celPuntos">
c k 0 3
t           </span></td>
t           <td>
c k 0 4
t           </td>
t         </tr>
t         <tr><td>
c k 1 1
t         </td><td>
c k 1 2
t         </td><td>
c k 1 3
t         </td><td>
c k 1 4
t         </td></tr>
t         <tr><td>
c k 2 1
t         </td><td>
c k 2 2
t         </td><td>
c k 2 3
t         </td><td>
c k 2 4
t         </td></tr>
t         <tr><td>
c k 3 1
t         </td><td>
c k 3 2
t         </td><td>
c k 3 3
t         </td><td>
c k 3 4
t         </td></tr>
t         <tr><td>
c k 4 1
t         </td><td>
c k 4 2
t         </td><td>
c k 4 3
t         </td><td>
c k 4 4
t         </td></tr>
t         <tr><td>
c k 5 1
t         </td><td>
c k 5 2
t         </td><td>
c k 5 3
t         </td><td>
c k 5 4
t         </td></tr>
t         <tr><td>
c k 6 1
t         </td><td>
c k 6 2
t         </td><td>
c k 6 3
t         </td><td>
c k 6 4
t         </td></tr>
t         <tr><td>
c k 7 1
t         </td><td>
c k 7 2
t         </td><td>
c k 7 3
t         </td><td>
c k 7 4
t         </td></tr>
t         <tr><td>
c k 8 1
t         </td><td>
c k 8 2
t         </td><td>
c k 8 3
t         </td><td>
c k 8 4
t         </td></tr>
t         <tr><td>
c k 9 1
t         </td><td>
c k 9 2
t         </td><td>
c k 9 3
t         </td><td>
c k 9 4
t         </td></tr>
t       </tbody>
t     </table>
t     <button class="btn" onclick="borrar()">Borrar Todos</button>
t   </div>
t </div>
t <script>
t   function borrar() {
t     fetch('records.cgi', {method: 'POST', body: new URLSearchParams({'ctrl': 'wipe_records'})})
t     .then(() => location.reload());
t   }
t </script>
t </body>
t </html>
.