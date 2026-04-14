t <!DOCTYPE html>
t <html lang="es">
t <head>
t <meta charset="UTF-8">
t t <title>Panel REACT - Nodo A</title>
t <style>
t   body { font-family: 'Segoe UI', Tahoma, Geneva, sans-serif; 
t          background-color: #eef2f5; color: #333; margin: 0; padding: 20px; }
t   .header { text-align: center; margin-bottom: 30px; }
t   .logo { max-width: 200px; border-radius: 10px; margin-bottom: 15px; }
t   .container { max-width: 800px; margin: 0 auto; background: #fff; 
t                padding: 30px; border-radius: 12px; 
t                box-shadow: 0 5px 15px rgba(0,0,0,0.1); }
t   h1 { color: #2c3e50; text-align: center; margin: 0; font-size: 2em; }
t   h2 { color: #34495e; border-bottom: 2px solid #3498db; 
t        padding-bottom: 5px; margin-top: 30px; }
t   .info-box { background-color: #f8f9fa; border-left: 5px solid #3498db; 
t               padding: 15px; margin-bottom: 20px; font-size: 1.1em; }
t   .form-group { margin-bottom: 20px; background: #fdfdfd; padding: 15px; border: 1px solid #eee; border-radius: 8px;}
t   select, input[type="text"] { padding: 10px; border: 1px solid #ccc; 
t                                border-radius: 5px; width: calc(100% - 22px); 
t                                max-width: 300px; margin-right: 10px; }
t   button { background-color: #3498db; color: white; padding: 10px 20px; 
t            border: none; border-radius: 5px; cursor: pointer; font-weight: bold; }
t   button:hover { background-color: #2980b9; }
t   .btn-sleep { background-color: #e67e22 !important; }
t   .btn-sleep:hover { background-color: #d35400 !important; }
t   .rx-box { background-color: #2c3e50; color: #2ecc71; padding: 15px; 
t             border-radius: 5px; font-family: 'Courier New', monospace; 
t             letter-spacing: 2px; font-size: 1.2em; text-align: center; }
t </style>
t </head>
t <body>
t <div class="container">
t   <div class="header">
t     <img src="logo.png" alt="REACT Logo" class="logo">
t     <h1>Estaci&oacute;n Base R.E.A.C.T.</h1>
t   </div>
t   
t   <h2>Monitorizaci&oacute;n en Tiempo Real</h2>
t   <div class="info-box" id="caja_estado">
t     <strong>Estado y Consumo (Nodo B):</strong><br>
c r 1
t   </div>
t   <div class="form-group">
t     <label><strong>&Uacute;ltima Trama (Eventos/Telemetr&iacute;a):</strong></label><br><br>
t     <div class="rx-box" id="caja_trama">
c r 2
t     </div>
t   </div>
t
t   <h2>Gestor de Entrenamiento Cognitivo</h2>
t   <form onsubmit="enviarComando(event, this)">
t     <div class="form-group">
t       <label><strong>Perfil de Usuario:</strong></label>
t       <select name="user_id" style="margin-bottom: 10px;">
t         <option value="0">0 - Invitado Local</option>
t         <option value="1">1 - Atleta A (Cristian)</option>
t         <option value="2">2 - Atleta B (Santiago)</option>
t       </select><br>
t       <label><strong>Seleccione Modo de Operaci&oacute;n:</strong></label><br>
t       <select name="modo">
t         <option value="1">1 - Memoria de Trabajo</option>
t         <option value="2">2 - Control de Fuerza</option>
t       </select>
t       <button type="submit">INICIAR PARTIDA</button>
t     </div>
t   </form>
t
t   <h2>Gesti&oacute;n de Energ&iacute;a</h2>
t   <form onsubmit="enviarComando(event, this)">
t     <div class="form-group" style="text-align: center;">
t       <input type="hidden" name="ctrl" value="sleep">
t       <button type="submit" class="btn-sleep">Forzar Bajo Consumo (Sleep)</button>
t     </div>
t   </form>
t </div>
t
t t <script>
t   // 1. Enviar formularios sin recargar la página
t   function enviarComando(e, form) {
t       e.preventDefault(); // Evita que la página salte
t       const formData = new URLSearchParams(new FormData(form));
t       fetch('panel.cgi', {
t           method: 'POST',
t           headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
t           body: formData.toString()
t       });
t   }
t
t   // 2. Pedir datos al Nodo A cada segundo silenciosamente
t   setInterval(function() {
t       fetch('panel.cgi')
t       .then(response => response.text())
t       .then(html => {
t           const parser = new DOMParser();
t           const doc = parser.parseFromString(html, 'text/html');
t           // Actualizamos SOLO las cajitas de texto
t           document.getElementById('caja_estado').innerHTML = doc.getElementById('caja_estado').innerHTML;
t           document.getElementById('caja_trama').innerHTML = doc.getElementById('caja_trama').innerHTML;
t       });
t   }, 1000);
t </script>
t </body>
t </html>
.