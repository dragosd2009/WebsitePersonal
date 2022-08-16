const username = document.getElementById('username')
const password = document.getElementById('password')
const form = document.getElementById('form')
const errorElement = document.getElementById('error')
form.addEventListener('submit', (e, event) => {
    let messages = []
    if (username.value == '' || username.value == null){
        messages.push('Error: Username is required!')
    }
    if (username.value.length <=6 ){
        messages.push('Error: Username must be longer!')
    
    }
    if (password.value.length <=6 ){
        messages.push('Error: Password must be longer!')
    
    }
    console.log(messages)
    if (password.value.length >=20 ){
        messages.push('Error: Password must be shorter!')
    }
    if (username.value == 'user' && password.value =="password"){
        
    }
    if (messages.length >0){
        e.preventDefault()
        errorElement.innerText = messages.join(', ')
    }
})